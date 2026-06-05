#include "pitch_detector.h"

#include <math.h>
#include <string.h>

#define DETECTOR_MIN_MIDI_NOTE 33
#define DETECTOR_MAX_MIDI_NOTE 96
#define DETECTOR_MIN_RMS 180.0f
#define DETECTOR_MIN_SCORE 0.035f
#define DETECTOR_MAX_SAMPLES 4096U
#define A4_FREQUENCY_HZ 440.0f
#define A4_MIDI_NOTE 69

/**
 * Clears a pitch detection result and sets default invalid fields.
 * @param result The result structure to reset.
 */
static void reset_result(PitchDetectionResult *result) {
    if (result == NULL) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->midi_note = -1;
}

/**
 * Clamps the input sample count to the detector's fixed stack buffer size.
 * @param sample_count The requested sample count.
 * @return The sample count safe for local buffers.
 */
static size_t clamp_sample_count(size_t sample_count) {
    return sample_count > DETECTOR_MAX_SAMPLES ? DETECTOR_MAX_SAMPLES : sample_count;
}

/**
 * Converts a MIDI note to its equal-tempered frequency.
 * @param midi_note The MIDI note number.
 * @return The frequency in Hz.
 */
static float midi_note_to_frequency(int midi_note) {
    return A4_FREQUENCY_HZ * powf(2.0f, ((float)midi_note - A4_MIDI_NOTE) / 12.0f);
}

/**
 * Rounds a MIDI note frequency to an integer frequency.
 * @param midi_note The MIDI note number.
 * @return The rounded frequency in Hz.
 */
static int midi_note_to_frequency_hz(int midi_note) {
    return (int)lroundf(midi_note_to_frequency(midi_note));
}

/**
 * Removes DC offset, computes raw RMS energy, and applies a Hann window.
 * @param samples The input PCM samples.
 * @param processed Receives floating-point samples for frequency analysis.
 * @param sample_count The number of samples to preprocess.
 * @param energy Receives the processed signal energy.
 * @return The raw RMS energy before windowing.
 */
static float preprocess_samples(
    const int16_t *samples,
    float *processed,
    size_t sample_count,
    double *energy
) {
    double mean = 0.0;
    double square_sum = 0.0;
    double windowed_energy = 0.0;
    size_t i;

    for (i = 0; i < sample_count; i++) {
        mean += samples[i];
    }
    mean /= (double)sample_count;

    for (i = 0; i < sample_count; i++) {
        float centered = (float)((double)samples[i] - mean);
        const float energy_sample = centered;

        if (sample_count > 1) {
            const float phase = (float)i / (float)(sample_count - 1U);
            const float window = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);
            centered *= window;
        }

        processed[i] = centered;
        square_sum += (double)energy_sample * (double)energy_sample;
        windowed_energy += (double)centered * (double)centered;
    }

    *energy = windowed_energy;
    return (float)sqrt(square_sum / (double)sample_count);
}

/**
 * Measures the energy around a target frequency using the Goertzel algorithm.
 * @param samples The preprocessed sample window.
 * @param sample_count The number of samples in the window.
 * @param sample_rate The sample rate in Hz.
 * @param frequency_hz The target frequency in Hz.
 * @param energy The total energy of the preprocessed sample window.
 * @return The normalized target-frequency score.
 */
static float goertzel_score(
    const float *samples,
    size_t sample_count,
    uint32_t sample_rate,
    float frequency_hz,
    double energy
) {
    const float omega = 2.0f * (float)M_PI * frequency_hz / (float)sample_rate;
    const float coefficient = 2.0f * cosf(omega);
    float previous = 0.0f;
    float previous2 = 0.0f;
    float power;
    size_t i;

    if (energy <= 0.0) {
        return 0.0f;
    }

    for (i = 0; i < sample_count; i++) {
        const float current = samples[i] + coefficient * previous - previous2;
        previous2 = previous;
        previous = current;
    }

    power = previous2 * previous2 + previous * previous - coefficient * previous * previous2;
    if (power <= 0.0f) {
        return 0.0f;
    }

    return power / ((float)energy * (float)sample_count);
}

int pitch_detect_note(
    const int16_t *samples,
    size_t sample_count,
    uint32_t sample_rate,
    PitchDetectionResult *result
) {
    float processed[DETECTOR_MAX_SAMPLES];
    double energy = 0.0;
    float best_score = 0.0f;
    int best_midi_note = -1;
    int midi_note;

    reset_result(result);

    if (samples == NULL || result == NULL || sample_count == 0 || sample_rate == 0) {
        return 0;
    }

    sample_count = clamp_sample_count(sample_count);
    result->rms = preprocess_samples(samples, processed, sample_count, &energy);
    if (result->rms < DETECTOR_MIN_RMS || energy <= 0.0) {
        return 0;
    }

    for (midi_note = DETECTOR_MIN_MIDI_NOTE; midi_note <= DETECTOR_MAX_MIDI_NOTE; midi_note++) {
        const float frequency_hz = midi_note_to_frequency(midi_note);
        float score;

        if (frequency_hz >= (float)sample_rate * 0.5f) {
            break;
        }

        score = goertzel_score(processed,
            sample_count,
            sample_rate,
            frequency_hz,
            energy);

        if (score > best_score) {
            best_score = score;
            best_midi_note = midi_note;
        }
    }

    if (best_midi_note < 0 || best_score < DETECTOR_MIN_SCORE) {
        result->confidence = best_score;
        return 0;
    }

    if (best_score > 1.0f) {
        best_score = 1.0f;
    }

    result->midi_note = best_midi_note;
    result->frequency_hz = midi_note_to_frequency_hz(best_midi_note);
    result->raw_frequency_hz = result->frequency_hz;
    result->confidence = best_score;
    result->voiced = true;
    result->fallback = false;

    return 1;
}

int pitch_detect_note_hz(
    const int16_t *samples,
    size_t sample_count,
    uint32_t sample_rate
) {
    PitchDetectionResult result;

    pitch_detect_note(samples, sample_count, sample_rate, &result);
    return result.frequency_hz;
}
