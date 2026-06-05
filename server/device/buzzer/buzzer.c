#include "buzzer.h"
#include "pitch_detector.h"
#include "wav_reader.h"

#include <pthread.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <softTone.h>
#include <wiringPi.h>

#define PITCH_WINDOW_MS 96U
#define PITCH_HOP_MS 20U
#define PITCH_MIN_WINDOW_SAMPLES 768U
#define PITCH_MIN_HOP_SAMPLES 128U
#define PITCH_MAX_WINDOW_SAMPLES 4096U
#define PITCH_HOLD_HOPS 4U
#define PITCH_CONFIRM_HOPS 1U
#define PITCH_JITTER_PERCENT 8U
#define MELODY_INITIAL_CAPACITY 64U
#define WAV_MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    int stable_pitch_hz;
    int stable_midi_note;
    int pending_pitch_hz;
    int pending_midi_note;
    unsigned int pending_hops;
    unsigned int missing_hops;
} PitchTracker;

typedef struct {
    int frequency_hz;
    unsigned int duration_ms;
} BuzzerNote;

typedef struct {
    BuzzerNote *notes;
    size_t count;
    size_t capacity;
} MelodyBuffer;

static pthread_mutex_t buzzer_lock = PTHREAD_MUTEX_INITIALIZER;
static bool buzzer_state = false;
static bool stop_requested = false;
static int current_tone_hz = -1;

/**
 * Writes a tone to the buzzer while the caller holds the buzzer lock.
 * @param tone_hz The output tone frequency, or 0 for silence.
 * @param state The logical buzzer on/off state to cache.
 */
static void write_output_locked(int tone_hz, bool state) {
    if (current_tone_hz != tone_hz) {
        softToneWrite(BUZZER_PIN, tone_hz);
        current_tone_hz = tone_hz;
    }
    buzzer_state = state;
}

/**
 * Stops buzzer output and optionally requests running playback to stop.
 * @param request_stop true to ask active playback loops to exit.
 */
static void stop_output(bool request_stop) {
    pthread_mutex_lock(&buzzer_lock);
    if (request_stop) {
        stop_requested = true;
    }
    write_output_locked(0, false);
    pthread_mutex_unlock(&buzzer_lock);
}

/**
 * Starts buzzer output at a fixed tone.
 * @param tone_hz The tone frequency to output.
 */
static void start_output(int tone_hz) {
    pthread_mutex_lock(&buzzer_lock);
    stop_requested = false;
    write_output_locked(tone_hz, tone_hz > 0);
    pthread_mutex_unlock(&buzzer_lock);
}

/**
 * Clears the playback stop request before starting a new music command.
 */
static void clear_stop_request(void) {
    pthread_mutex_lock(&buzzer_lock);
    stop_requested = false;
    pthread_mutex_unlock(&buzzer_lock);
}

/**
 * Writes a tone unless a stop request has been received.
 * @param tone_hz The tone frequency to output, or 0 for silence.
 * @return true if playback should continue, false if it should stop.
 */
static bool write_output_if_running(int tone_hz) {
    pthread_mutex_lock(&buzzer_lock);
    if (stop_requested) {
        write_output_locked(0, false);
        pthread_mutex_unlock(&buzzer_lock);
        return false;
    }

    write_output_locked(tone_hz, tone_hz > 0);
    pthread_mutex_unlock(&buzzer_lock);
    return true;
}

/**
 * Checks whether another command requested buzzer playback to stop.
 * @return true when playback should stop, false otherwise.
 */
static bool is_stop_requested(void) {
    bool requested;

    pthread_mutex_lock(&buzzer_lock);
    requested = stop_requested;
    pthread_mutex_unlock(&buzzer_lock);

    return requested;
}

/**
 * Delays in short steps so stop requests can interrupt playback.
 * @param duration_ms The total delay duration in milliseconds.
 * @return true if the delay completed, false if playback was stopped.
 */
static bool delay_until_done_or_stopped(unsigned int duration_ms) {
    unsigned int elapsed_ms = 0;

    while (elapsed_ms < duration_ms) {
        unsigned int step_ms = duration_ms - elapsed_ms;
        if (step_ms > 10U) {
            step_ms = 10U;
        }

        if (is_stop_requested()) {
            return false;
        }

        delay(step_ms);
        elapsed_ms += step_ms;
    }

    return !is_stop_requested();
}

/**
 * Checks whether two pitch values are close enough to treat as the same note.
 * @param pitch_hz The new pitch value.
 * @param previous_pitch_hz The previous stable pitch value.
 * @return true if the pitch difference is within the jitter tolerance.
 */
static bool pitches_are_close(int pitch_hz, int previous_pitch_hz) {
    int difference;

    if (pitch_hz <= 0 || previous_pitch_hz <= 0) {
        return false;
    }

    difference = pitch_hz > previous_pitch_hz
        ? pitch_hz - previous_pitch_hz
        : previous_pitch_hz - pitch_hz;

    return difference * 100 <= previous_pitch_hz * PITCH_JITTER_PERCENT;
}

/**
 * Initializes pitch tracker state before music playback begins.
 * @param tracker The tracker to initialize.
 */
static void pitch_tracker_init(PitchTracker *tracker) {
    memset(tracker, 0, sizeof(*tracker));
    tracker->stable_midi_note = -1;
    tracker->pending_midi_note = -1;
}

/**
 * Clears a candidate note that has not yet been confirmed.
 * @param tracker The tracker to update.
 */
static void pitch_tracker_clear_pending(PitchTracker *tracker) {
    tracker->pending_pitch_hz = 0;
    tracker->pending_midi_note = -1;
    tracker->pending_hops = 0;
}

/**
 * Holds the last stable pitch briefly, then falls back to silence.
 * @param tracker The tracker to update.
 * @return The pitch to output, or 0 for silence.
 */
static int pitch_tracker_hold_or_silence(PitchTracker *tracker) {
    pitch_tracker_clear_pending(tracker);

    if (tracker->stable_pitch_hz > 0 && tracker->missing_hops < PITCH_HOLD_HOPS) {
        tracker->missing_hops += 1;
        return tracker->stable_pitch_hz;
    }

    tracker->stable_pitch_hz = 0;
    tracker->stable_midi_note = -1;
    tracker->missing_hops = 0;
    return 0;
}

/**
 * Smooths a raw pitch-detection result into a stable output pitch.
 * @param tracker The tracker state.
 * @param detection The latest pitch-detection result.
 * @return The pitch to output, or 0 for silence.
 */
static int pitch_tracker_update(PitchTracker *tracker, const PitchDetectionResult *detection) {
    if (!detection->voiced || detection->frequency_hz <= 0) {
        return pitch_tracker_hold_or_silence(tracker);
    }

    if (tracker->stable_pitch_hz > 0 &&
        (detection->midi_note == tracker->stable_midi_note ||
            pitches_are_close(detection->frequency_hz, tracker->stable_pitch_hz))) {
        tracker->missing_hops = 0;
        pitch_tracker_clear_pending(tracker);
        return tracker->stable_pitch_hz;
    }

    if (detection->fallback) {
        return pitch_tracker_hold_or_silence(tracker);
    }

    if (detection->midi_note == tracker->pending_midi_note) {
        tracker->pending_hops += 1;
    }
    else {
        tracker->pending_pitch_hz = detection->frequency_hz;
        tracker->pending_midi_note = detection->midi_note;
        tracker->pending_hops = 1;
    }

    if (tracker->pending_hops >= PITCH_CONFIRM_HOPS) {
        tracker->stable_pitch_hz = tracker->pending_pitch_hz;
        tracker->stable_midi_note = tracker->pending_midi_note;
        tracker->missing_hops = 0;
        pitch_tracker_clear_pending(tracker);
        return tracker->stable_pitch_hz;
    }

    return tracker->stable_pitch_hz;
}

/**
 * Checks whether pitch debug logging is enabled by environment variable.
 * @return true if BUZZER_PITCH_DEBUG is set to a non-zero value.
 */
static bool pitch_debug_enabled(void) {
    const char *value = getenv("BUZZER_PITCH_DEBUG");
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

/**
 * Logs one pitch tracking step when debug logging is enabled.
 * @param detection The raw pitch-detection result.
 * @param output_hz The final smoothed output pitch.
 */
static void log_pitch_debug(const PitchDetectionResult *detection, int output_hz) {
    if (!pitch_debug_enabled()) {
        return;
    }

    fprintf(stderr,
        "pitch raw_hz=%d note_hz=%d midi=%d confidence=%.3f rms=%.1f voiced=%d fallback=%d output_hz=%d\n",
        detection->raw_frequency_hz,
        detection->frequency_hz,
        detection->midi_note,
        detection->confidence,
        detection->rms,
        detection->voiced ? 1 : 0,
        detection->fallback ? 1 : 0,
        output_hz);
}

/**
 * Converts a duration to a clamped sample count.
 * @param sample_rate The WAV sample rate.
 * @param duration_ms The duration in milliseconds.
 * @param min_samples The minimum returned sample count.
 * @param max_samples The maximum returned sample count.
 * @return The clamped sample count.
 */
static size_t samples_for_duration(
    uint32_t sample_rate,
    unsigned int duration_ms,
    size_t min_samples,
    size_t max_samples
) {
    uint64_t samples;

    samples = ((uint64_t)sample_rate * duration_ms + 999U) / 1000U;
    if (samples < min_samples) {
        samples = min_samples;
    }
    if (samples > max_samples) {
        samples = max_samples;
    }

    return (size_t)samples;
}

/**
 * Converts a sample count to a playback duration in milliseconds.
 * @param sample_count The number of samples to play.
 * @param sample_rate The WAV sample rate.
 * @return The rounded-up duration in milliseconds.
 */
static unsigned int samples_to_duration_ms(size_t sample_count, uint32_t sample_rate) {
    uint64_t duration_ms;

    duration_ms = ((uint64_t)sample_count * 1000U + sample_rate - 1U) / sample_rate;
    if (duration_ms == 0) {
        duration_ms = 1;
    }

    return (unsigned int)duration_ms;
}

/**
 * Reads more WAV samples into a sliding analysis window.
 * @param file The WAV file stream.
 * @param info The parsed WAV metadata.
 * @param samples The destination sample window.
 * @param offset The write offset inside the window.
 * @param capacity The total window capacity.
 * @param remaining_data_size The remaining WAV data byte count.
 * @return The number of samples read.
 */
static size_t read_window_samples(
    FILE *file,
    const WavInfo *info,
    int16_t *samples,
    size_t offset,
    size_t capacity,
    uint32_t *remaining_data_size
) {
    size_t frame_size;
    size_t available_samples;
    size_t request_samples;
    int samples_read;

    if (offset >= capacity) {
        return 0;
    }

    frame_size = wav_frame_size(info);
    if (frame_size == 0) {
        return 0;
    }

    available_samples = *remaining_data_size / frame_size;
    request_samples = WAV_MIN(capacity - offset, available_samples);
    if (request_samples == 0) {
        return 0;
    }

    samples_read = wav_read_samples(file,
        info,
        samples + offset,
        request_samples,
        remaining_data_size);
    if (samples_read <= 0) {
        return 0;
    }

    return (size_t)samples_read;
}

/**
 * Releases memory owned by a melody buffer.
 * @param melody The melody buffer to clear.
 */
static void melody_buffer_cleanup(MelodyBuffer *melody) {
    free(melody->notes);
    melody->notes = NULL;
    melody->count = 0;
    melody->capacity = 0;
}

/**
 * Ensures a melody buffer can store at least one more note.
 * @param melody The melody buffer to grow.
 * @return DEVICE_RESULT_OK on success, or an error code on allocation failure.
 */
static int melody_buffer_reserve_next(MelodyBuffer *melody) {
    BuzzerNote *new_notes;
    size_t new_capacity;

    if (melody->count < melody->capacity) {
        return DEVICE_RESULT_OK;
    }

    new_capacity = melody->capacity == 0 ? MELODY_INITIAL_CAPACITY : melody->capacity * 2U;
    new_notes = realloc(melody->notes, new_capacity * sizeof(melody->notes[0]));
    if (new_notes == NULL) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    melody->notes = new_notes;
    melody->capacity = new_capacity;
    return DEVICE_RESULT_OK;
}

/**
 * Appends a note to a melody buffer, merging adjacent equal frequencies.
 * @param melody The melody buffer to append to.
 * @param frequency_hz The note frequency, or 0 for silence.
 * @param duration_ms The note duration in milliseconds.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int melody_buffer_append(
    MelodyBuffer *melody,
    int frequency_hz,
    unsigned int duration_ms
) {
    int result;

    if (duration_ms == 0) {
        return DEVICE_RESULT_OK;
    }

    if (frequency_hz < 0) {
        frequency_hz = 0;
    }

    if (melody->count > 0 &&
        melody->notes[melody->count - 1U].frequency_hz == frequency_hz) {
        melody->notes[melody->count - 1U].duration_ms += duration_ms;
        return DEVICE_RESULT_OK;
    }

    result = melody_buffer_reserve_next(melody);
    if (result != DEVICE_RESULT_OK) {
        return result;
    }

    melody->notes[melody->count].frequency_hz = frequency_hz;
    melody->notes[melody->count].duration_ms = duration_ms;
    melody->count += 1U;
    return DEVICE_RESULT_OK;
}

/**
 * Converts an opened WAV file into a precomputed buzzer-note array.
 * @param file The WAV file stream positioned at the data chunk.
 * @param info The parsed WAV metadata.
 * @param melody The melody buffer to populate.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
static int build_melody_from_wav(
    FILE *file,
    const WavInfo *info,
    MelodyBuffer *melody
) {
    uint32_t remaining_data_size = info->data_size;
    int16_t samples[PITCH_MAX_WINDOW_SAMPLES];
    size_t window_samples;
    size_t hop_samples;
    size_t active_samples;
    PitchTracker pitch_tracker;

    pitch_tracker_init(&pitch_tracker);

    window_samples = samples_for_duration(info->sample_rate,
        PITCH_WINDOW_MS,
        PITCH_MIN_WINDOW_SAMPLES,
        PITCH_MAX_WINDOW_SAMPLES);
    hop_samples = samples_for_duration(info->sample_rate,
        PITCH_HOP_MS,
        PITCH_MIN_HOP_SAMPLES,
        window_samples);
    if (hop_samples >= window_samples) {
        hop_samples = window_samples / 2U;
    }
    if (hop_samples == 0) {
        hop_samples = 1;
    }

    active_samples = read_window_samples(file,
        info,
        samples,
        0,
        window_samples,
        &remaining_data_size);

    while (active_samples > 0) {
        PitchDetectionResult detection;
        int pitch_hz;
        int result;
        size_t play_samples;
        unsigned int duration_ms;

        if (is_stop_requested()) {
            return DEVICE_RESULT_OK;
        }

        pitch_detect_note(samples, active_samples, info->sample_rate, &detection);
        pitch_hz = pitch_tracker_update(&pitch_tracker, &detection);
        log_pitch_debug(&detection, pitch_hz);

        play_samples = WAV_MIN(hop_samples, active_samples);
        duration_ms = samples_to_duration_ms(play_samples, info->sample_rate);

        result = melody_buffer_append(melody, pitch_hz, duration_ms);
        if (result != DEVICE_RESULT_OK) {
            return result;
        }

        if (remaining_data_size == 0 && active_samples <= play_samples) {
            break;
        }

        if (play_samples < active_samples) {
            memmove(samples,
                samples + play_samples,
                (active_samples - play_samples) * sizeof(samples[0]));
            active_samples -= play_samples;
        }
        else {
            active_samples = 0;
        }

        active_samples += read_window_samples(file,
            info,
            samples,
            active_samples,
            window_samples,
            &remaining_data_size);
    }

    return DEVICE_RESULT_OK;
}

/**
 * Plays a precomputed melody note array through softTone.
 * @param melody The precomputed melody to play.
 * @return DEVICE_RESULT_OK when playback finishes or is stopped.
 */
static int play_melody(const MelodyBuffer *melody) {
    size_t i;

    for (i = 0; i < melody->count; i++) {
        const BuzzerNote *note = &melody->notes[i];

        if (!write_output_if_running(note->frequency_hz)) {
            break;
        }

        if (!delay_until_done_or_stopped(note->duration_ms)) {
            break;
        }
    }

    stop_output(false);
    return DEVICE_RESULT_OK;
}

int buzzer_init(void) {
    if (wiringPiSetupGpio() == -1) {
        return DEVICE_RESULT_GPIO_SETUP_FAILED;
    }

    if (softToneCreate(BUZZER_PIN) != 0) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    stop_output(false);

    return DEVICE_RESULT_OK;
}

void buzzer_cleanup(void) {
    stop_output(true);
}

int buzzer_music(char *file_path) {
    FILE *file;
    WavInfo info;
    int result;
    MelodyBuffer melody = { 0 };

    if (file_path == NULL || file_path[0] == '\0') {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    clear_stop_request();
    stop_output(false);

    file = fopen(file_path, "rb");
    if (file == NULL) {
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    result = wav_read_header(file, &info);
    if (result != DEVICE_RESULT_OK) {
        fclose(file);
        return result;
    }

    if (fseek(file, (long)info.data_offset, SEEK_SET) != 0) {
        fclose(file);
        return DEVICE_RESULT_DEVICE_FAILED;
    }

    result = build_melody_from_wav(file, &info, &melody);
    fclose(file);
    if (result != DEVICE_RESULT_OK) {
        melody_buffer_cleanup(&melody);
        stop_output(false);
        return result;
    }

    result = play_melody(&melody);
    melody_buffer_cleanup(&melody);
    return result;
}

int buzzer_on(void) {
    start_output(BUZZER_DEFAULT_TONE);

    return DEVICE_RESULT_OK;
}

int buzzer_off(void) {
    stop_output(true);

    return DEVICE_RESULT_OK;
}

int buzzer_beep(int duration_ms) {
    if (duration_ms <= 0 || duration_ms > 10000) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    start_output(BUZZER_DEFAULT_TONE);
    if (!delay_until_done_or_stopped((unsigned int)duration_ms)) {
        stop_output(false);
        return DEVICE_RESULT_OK;
    }

    stop_output(false);
    return DEVICE_RESULT_OK;
}

bool buzzer_is_on(void) {
    bool state;

    pthread_mutex_lock(&buzzer_lock);
    state = buzzer_state;
    pthread_mutex_unlock(&buzzer_lock);

    return state;
}
