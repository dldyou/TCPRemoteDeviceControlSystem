#include "pitch_detector.h"

#include <math.h>
#include <string.h>

#define YIN_MIN_PITCH_HZ 80U
#define YIN_MAX_PITCH_HZ 2000U
#define YIN_THRESHOLD 0.20f
#define YIN_FALLBACK_THRESHOLD 0.35f
#define YIN_MIN_RMS 400.0f
#define YIN_MAX_SAMPLES 2048U
#define YIN_MAX_TAU 1024U
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
    return sample_count > YIN_MAX_SAMPLES ? YIN_MAX_SAMPLES : sample_count;
}

/**
 * Removes DC offset, applies a Hann window, and computes RMS energy.
 * @param samples The input PCM samples.
 * @param processed Receives normalized floating-point samples.
 * @param sample_count The number of samples to preprocess.
 * @return The RMS energy of the processed window.
 */
static float preprocess_samples(
    const int16_t *samples,
    float *processed,
    size_t sample_count
) {
    double mean = 0.0;
    double square_sum = 0.0;
    size_t i;

    for (i = 0; i < sample_count; i++) {
        mean += samples[i];
    }
    mean /= (double)sample_count;

    for (i = 0; i < sample_count; i++) {
        float centered = (float)((double)samples[i] - mean);

        if (sample_count > 1) {
            const float phase = (float)i / (float)(sample_count - 1U);
            const float window = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);
            centered *= window;
        }

        processed[i] = centered;
        square_sum += (double)centered * (double)centered;
    }

    return (float)sqrt(square_sum / (double)sample_count);
}

/**
 * Fills the YIN difference function for every tested lag.
 * @param yin The YIN buffer to write.
 * @param samples The preprocessed input samples.
 * @param sample_count The number of input samples.
 * @param max_tau The maximum lag to test.
 */
static void fill_difference_function(
    float *yin,
    const float *samples,
    size_t sample_count,
    size_t max_tau
) {
    size_t tau;

    yin[0] = 0.0f;

    for (tau = 1; tau <= max_tau; tau++) {
        float difference = 0.0f;
        size_t i;

        for (i = 0; i + tau < sample_count; i++) {
            const float delta = samples[i] - samples[i + tau];
            difference += delta * delta;
        }

        yin[tau] = difference;
    }
}

/**
 * Applies cumulative mean normalized difference to the YIN buffer.
 * @param yin The YIN buffer to normalize in place.
 * @param max_tau The maximum lag stored in the buffer.
 */
static void normalize_difference_function(float *yin, size_t max_tau) {
    float running_sum = 0.0f;
    size_t tau;

    yin[0] = 1.0f;

    for (tau = 1; tau <= max_tau; tau++) {
        running_sum += yin[tau];

        if (running_sum == 0.0f) {
            yin[tau] = 1.0f;
        }
        else {
            yin[tau] = yin[tau] * (float)tau / running_sum;
        }
    }
}

/**
 * Finds the first lag that passes the primary YIN threshold.
 * @param yin The normalized YIN buffer.
 * @param min_tau The minimum allowed lag.
 * @param max_tau The maximum allowed lag.
 * @return The detected lag, or 0 if none is found.
 */
static size_t find_first_pitch_tau(
    const float *yin,
    size_t min_tau,
    size_t max_tau
) {
    size_t tau;

    for (tau = min_tau; tau <= max_tau; tau++) {
        if (yin[tau] < YIN_THRESHOLD) {
            while (tau + 1 <= max_tau && yin[tau + 1] < yin[tau]) {
                tau++;
            }
            return tau;
        }
    }

    return 0;
}

/**
 * Finds a weaker fallback lag for holding existing notes.
 * @param yin The normalized YIN buffer.
 * @param min_tau The minimum allowed lag.
 * @param max_tau The maximum allowed lag.
 * @return The fallback lag, or 0 if confidence is too low.
 */
static size_t find_best_fallback_tau(
    const float *yin,
    size_t min_tau,
    size_t max_tau
) {
    float best_value = 1.0f;
    size_t best_tau = 0;
    size_t tau;

    for (tau = min_tau; tau <= max_tau; tau++) {
        if (yin[tau] < best_value) {
            best_value = yin[tau];
            best_tau = tau;
        }
    }

    if (best_tau == 0 || best_value > YIN_FALLBACK_THRESHOLD) {
        return 0;
    }

    return best_tau;
}

/**
 * Refines a detected lag using parabolic interpolation.
 * @param yin The normalized YIN buffer.
 * @param tau The integer lag to refine.
 * @param max_tau The maximum valid lag.
 * @return The refined fractional lag.
 */
static float refine_tau_with_parabolic_interpolation(
    const float *yin,
    size_t tau,
    size_t max_tau
) {
    float better_tau = (float)tau;

    if (tau > 0 && tau + 1 <= max_tau) {
        const float left = yin[tau - 1];
        const float center = yin[tau];
        const float right = yin[tau + 1];
        const float denominator = left - 2.0f * center + right;

        if (fabsf(denominator) > 0.000001f) {
            better_tau += 0.5f * (left - right) / denominator;
        }
    }

    return better_tau;
}

/**
 * Converts a frequency to the nearest equal-tempered MIDI note.
 * @param frequency_hz The frequency in Hz.
 * @return The nearest MIDI note number.
 */
static int frequency_to_midi_note(float frequency_hz) {
    return (int)lroundf(12.0f * log2f(frequency_hz / A4_FREQUENCY_HZ) + A4_MIDI_NOTE);
}

/**
 * Converts a MIDI note to its equal-tempered frequency.
 * @param midi_note The MIDI note number.
 * @return The rounded frequency in Hz.
 */
static int midi_note_to_frequency(int midi_note) {
    const float frequency = A4_FREQUENCY_HZ * powf(2.0f, ((float)midi_note - A4_MIDI_NOTE) / 12.0f);
    return (int)lroundf(frequency);
}

int pitch_detect_yin(
    const int16_t *samples,
    size_t sample_count,
    uint32_t sample_rate,
    PitchDetectionResult *result
) {
    float processed[YIN_MAX_SAMPLES];
    float yin[YIN_MAX_TAU + 1U];
    size_t min_tau;
    size_t max_tau;
    size_t best_tau;
    float better_tau;
    float raw_frequency;

    reset_result(result);

    if (samples == NULL || result == NULL || sample_count == 0 || sample_rate == 0) {
        return 0;
    }

    sample_count = clamp_sample_count(sample_count);
    result->rms = preprocess_samples(samples, processed, sample_count);
    if (result->rms < YIN_MIN_RMS) {
        return 0;
    }

    min_tau = sample_rate / YIN_MAX_PITCH_HZ;
    max_tau = sample_rate / YIN_MIN_PITCH_HZ;

    if (min_tau < 2U) {
        min_tau = 2U;
    }
    if (max_tau > YIN_MAX_TAU) {
        max_tau = YIN_MAX_TAU;
    }
    if (max_tau >= sample_count / 2U) {
        max_tau = sample_count / 2U;
    }
    if (max_tau <= min_tau) {
        return 0;
    }

    fill_difference_function(yin, processed, sample_count, max_tau);
    normalize_difference_function(yin, max_tau);

    best_tau = find_first_pitch_tau(yin, min_tau, max_tau);
    if (best_tau == 0) {
        best_tau = find_best_fallback_tau(yin, min_tau, max_tau);
        result->fallback = best_tau != 0;
    }

    if (best_tau == 0) {
        return 0;
    }

    better_tau = refine_tau_with_parabolic_interpolation(yin, best_tau, max_tau);
    if (better_tau <= 0.0f) {
        return 0;
    }

    raw_frequency = (float)sample_rate / better_tau;
    if (raw_frequency < YIN_MIN_PITCH_HZ || raw_frequency > YIN_MAX_PITCH_HZ) {
        return 0;
    }

    result->raw_frequency_hz = (int)lroundf(raw_frequency);
    result->confidence = 1.0f - yin[best_tau];
    if (result->confidence < 0.0f) {
        result->confidence = 0.0f;
    }
    if (result->confidence > 1.0f) {
        result->confidence = 1.0f;
    }

    result->midi_note = frequency_to_midi_note(raw_frequency);
    result->frequency_hz = midi_note_to_frequency(result->midi_note);
    result->voiced = true;

    return 1;
}

int pitch_detect_yin_hz(
    const int16_t *samples,
    size_t sample_count,
    uint32_t sample_rate
) {
    PitchDetectionResult result;

    pitch_detect_yin(samples, sample_count, sample_rate, &result);
    return result.frequency_hz;
}
