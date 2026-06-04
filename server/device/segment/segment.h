#pragma once

#include <stdbool.h>

#include "../device_result.h"

#define SEGMENT_COUNT 8

#ifndef SEGMENT_A_PIN
#define SEGMENT_A_PIN 5
#endif

#ifndef SEGMENT_B_PIN
#define SEGMENT_B_PIN 6
#endif

#ifndef SEGMENT_C_PIN
#define SEGMENT_C_PIN 13
#endif

#ifndef SEGMENT_D_PIN
#define SEGMENT_D_PIN 19
#endif

#ifndef SEGMENT_E_PIN
#define SEGMENT_E_PIN 26
#endif

#ifndef SEGMENT_F_PIN
#define SEGMENT_F_PIN 20
#endif

#ifndef SEGMENT_G_PIN
#define SEGMENT_G_PIN 21
#endif

#ifndef SEGMENT_DP_PIN
#define SEGMENT_DP_PIN 16
#endif

/**
 * Initializes all seven-segment GPIO pins.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int segment_init(void);

/**
 * Clears the display and resets cached segment state.
 */
void segment_cleanup(void);

/**
 * Displays a single digit on the seven-segment display.
 * @param value The digit value from 0 to 9.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int segment_display(int value);

/**
 * Clears the seven-segment display.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int segment_clear(void);

/**
 * Reads whether the display is currently enabled.
 * @return true if a digit is currently displayed, false otherwise.
 */
bool segment_is_enabled(void);

/**
 * Reads the cached displayed digit value.
 * @return The cached digit value.
 */
int segment_get_value(void);
