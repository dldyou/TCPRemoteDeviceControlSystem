#pragma once

#include "../device_result.h"

#ifndef LIGHT_PIN
#define LIGHT_PIN 27
#endif

/**
 * Initializes the light sensor GPIO pin.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int light_init(void);

/**
 * Resets the cached light sensor state.
 */
void light_cleanup(void);

/**
 * Reads the current light sensor value.
 * @param value Receives the sensor value when non-NULL.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int light_read(int *value);

/**
 * Returns the last cached light sensor value.
 * @return The last light value, or -1 if no value has been read.
 */
int light_get_last_value(void);
