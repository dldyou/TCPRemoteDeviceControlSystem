#include "device_result.h"

const char *device_result_message(int result) {
    switch (result) {
    case DEVICE_RESULT_OK:
        return "OK";
    case DEVICE_RESULT_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case DEVICE_RESULT_GPIO_SETUP_FAILED:
        return "GPIO_SETUP_FAILED";
    case DEVICE_RESULT_THREAD_FAILED:
        return "THREAD_FAILED";
    case DEVICE_RESULT_INVALID_VALUE:
        return "INVALID_VALUE";
    case DEVICE_RESULT_PLUGIN_LOAD_FAILED:
        return "PLUGIN_LOAD_FAILED";
    case DEVICE_RESULT_SYMBOL_LOAD_FAILED:
        return "SYMBOL_LOAD_FAILED";
    case DEVICE_RESULT_DEVICE_FAILED:
        return "DEVICE_FAILED";
    case DEVICE_RESULT_CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}
