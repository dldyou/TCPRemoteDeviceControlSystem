#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int frequency_hz;
    int raw_frequency_hz;
    int midi_note;
    float confidence;
    float rms;
    bool voiced;
    bool fallback;
} PitchDetectionResult;

/**
 * Detects the strongest musical note in a PCM sample window.
 * @param samples The input signed 16-bit mono samples.
 * @param sample_count The number of input samples.
 * @param sample_rate The input sample rate in Hz.
 * @param result Receives frequency, confidence, RMS, and voiced state.
 * @return 1 when a voiced pitch is detected, 0 otherwise.
 */
int pitch_detect_note(
    const int16_t *samples,
    size_t sample_count,
    uint32_t sample_rate,
    PitchDetectionResult *result
);

/**
 * Detects the strongest musical note and returns only its frequency.
 * @param samples The input signed 16-bit mono samples.
 * @param sample_count The number of input samples.
 * @param sample_rate The input sample rate in Hz.
 * @return The detected frequency in Hz, or 0 when no pitch is detected.
 */
int pitch_detect_note_hz(
    const int16_t *samples,
    size_t sample_count,
    uint32_t sample_rate
);
