#pragma once

#include <stdbool.h>

#include "../device_result.h"

#ifndef BUZZER_PIN
#define BUZZER_PIN 18
#endif

#ifndef BUZZER_DEFAULT_TONE
#define BUZZER_DEFAULT_TONE 440
#endif

/**
 * Initializes the buzzer GPIO pin and softTone output.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int buzzer_init(void);

/**
 * Stops buzzer output and releases plugin state.
 */
void buzzer_cleanup(void);

/**
 * Plays a WAV-derived melody through the buzzer.
 * @param file_path The WAV file path to read.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int buzzer_music(char *file_path);

/**
 * Turns the buzzer on at the default tone.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int buzzer_on(void);

/**
 * Turns the buzzer off and interrupts music playback.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int buzzer_off(void);

/**
 * Plays a fixed-tone beep for a requested duration.
 * @param duration_ms The beep duration in milliseconds.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int buzzer_beep(int duration_ms);

/**
 * Reads the cached buzzer on/off state.
 * @return true if the buzzer is logically on, false otherwise.
 */
bool buzzer_is_on(void);
