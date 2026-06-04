#pragma once

typedef enum {
    DEVICE_RESULT_OK = 0,
    DEVICE_RESULT_NOT_INITIALIZED = -1,
    DEVICE_RESULT_GPIO_SETUP_FAILED = -2,
    DEVICE_RESULT_THREAD_FAILED = -3,
    DEVICE_RESULT_INVALID_VALUE = -4,
    DEVICE_RESULT_PLUGIN_LOAD_FAILED = -5,
    DEVICE_RESULT_SYMBOL_LOAD_FAILED = -6,
    DEVICE_RESULT_DEVICE_FAILED = -7,
    DEVICE_RESULT_CANCELLED = -8
} DeviceResult;

/**
 * Converts a device result code to a readable message.
 * @param result The device result code.
 * @return A static string describing the result.
 */
const char *device_result_message(int result);
