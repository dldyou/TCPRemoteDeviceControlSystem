#include "parsing.h"

#include <signal.h>
#include <stdio.h>

#include "app_config.h"
#include "command.h"
#include "device/device_manager.h"

/**
 * Writes a normalized device error response into the response buffer.
 * @param str The response buffer to write into.
 * @param result The device result code to report.
 */
static void write_device_error(char *str, int result) {
    snprintf(str, BUFFER_SIZE, "ERR DEVICE_ERROR %s\n", device_result_message(result));
}

void exec_command(char *str) {
    ParsedCommand command = command_parse(str);
    DeviceStatus status;
    PluginStatus plugin_status;
    int result;
    int light_value;

    switch (command.type) {
    case COMMAND_HELP:
        snprintf(str, BUFFER_SIZE,
            "OK COMMANDS: HELP, STATUS, PLUGIN LIST, LED ON|BRIGHT <0-255>|OFF, BUZZER ON|OFF|BEEP <ms>|MUSIC <file>, LIGHT READ, SEG DISPLAY <0-9>|CLEAR, QUIT\n"
        );
        break;

    case COMMAND_STATUS:
        status = device_get_status();
        snprintf(str, BUFFER_SIZE,
            "DATA STATUS INIT=%s LED=%s BUZZER=%s LIGHT=%d SEG=%s SEG_VALUE=%d\n",
            status.initialized ? "ON" : "OFF",
            status.led_on ? "ON" : "OFF",
            status.buzzer_on ? "ON" : "OFF",
            status.light_value,
            status.segment_enabled ? "ON" : "OFF",
            status.segment_value
        );
        break;

    case COMMAND_PLUGIN_LIST:
        plugin_status = plugin_get_status();
        snprintf(str, BUFFER_SIZE,
            "DATA PLUGINS LED=%s BUZZER=%s LIGHT=%s SEGMENT=%s\n",
            plugin_status.led_loaded ? "LOADED" : "MISSING",
            plugin_status.buzzer_loaded ? "LOADED" : "MISSING",
            plugin_status.light_loaded ? "LOADED" : "MISSING",
            plugin_status.segment_loaded ? "LOADED" : "MISSING"
        );
        break;

    case COMMAND_QUIT:
        snprintf(str, BUFFER_SIZE, "OK QUIT\n");
        raise(SIGTERM);
        break;

    case COMMAND_LED_ON:
        result = device_led_on();
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK LED ON\n");
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_LED_ON_BRIGHT:
        result = device_led_on_bright(command.value);
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK LED BRIGHT %d\n", command.value);
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_LED_OFF:
        result = device_led_off();
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK LED OFF\n");
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_BUZZER_MUSIC:
        result = device_buzzer_music(command.str);
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK BUZZER MUSIC\n");
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_BUZZER_ON:
        result = device_buzzer_on();
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK BUZZER ON\n");
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_BUZZER_OFF:
        result = device_buzzer_off();
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK BUZZER OFF\n");
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_BUZZER_BEEP:
        result = device_buzzer_beep(command.value);
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK BUZZER BEEP %d\n", command.value);
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_LIGHT_READ:
        result = device_light_read(&light_value);
        if (result == DEVICE_RESULT_OK) {
            if (light_value == 0) {
                result = device_led_on();
            }
            else if (light_value == 1) {
                result = device_led_off();
            }

            if (result == DEVICE_RESULT_OK) {
                snprintf(str, BUFFER_SIZE, "DATA LIGHT %d LED=%s\n",
                         light_value,
                         light_value == 0 ? "ON" : "OFF");
            }
            else {
                write_device_error(str, result);
            }
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_SEG_DISPLAY:
        result = device_segment_display(command.value);
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK SEG DISPLAY %d\n", command.value);
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_SEG_CLEAR:
        result = device_segment_clear();
        if (result == DEVICE_RESULT_OK) {
            snprintf(str, BUFFER_SIZE, "OK SEG CLEAR\n");
        }
        else {
            write_device_error(str, result);
        }
        break;

    case COMMAND_EMPTY:
        snprintf(str, BUFFER_SIZE, "ERR EMPTY_COMMAND\n");
        break;

    case COMMAND_INVALID_VALUE:
        snprintf(str, BUFFER_SIZE, "ERR INVALID_VALUE\n");
        break;

    case COMMAND_INVALID:
    default:
        snprintf(str, BUFFER_SIZE, "ERR INVALID_COMMAND\n");
        break;
    }
}
