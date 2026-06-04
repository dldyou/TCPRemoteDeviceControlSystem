#include "command.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define COMMAND_LINE_MAX 256

typedef enum {
    COMMAND_ARG_NONE,
    COMMAND_ARG_INT,
    COMMAND_ARG_STRING,
} CommandArgType;

typedef struct {
    const char *device;
    const char *action;
    Command command;
    CommandArgType arg_type;
    int min_value;
    int max_value;
} CommandSpec;

static const CommandSpec command_specs[] = {
    { "HELP", NULL, COMMAND_HELP, COMMAND_ARG_NONE, 0, 0 },
    { "STATUS", NULL, COMMAND_STATUS, COMMAND_ARG_NONE, 0, 0 },
    { "PLUGIN", "LIST", COMMAND_PLUGIN_LIST, COMMAND_ARG_NONE, 0, 0 },
    { "DEVICE", "LIST", COMMAND_PLUGIN_LIST, COMMAND_ARG_NONE, 0, 0 },
    { "QUIT", NULL, COMMAND_QUIT, COMMAND_ARG_NONE, 0, 0 },

    { "LED", "ON", COMMAND_LED_ON, COMMAND_ARG_NONE, 0, 0 },
    { "LED", "BRIGHT", COMMAND_LED_ON_BRIGHT, COMMAND_ARG_INT, 0, 255 },
    { "LED", "OFF", COMMAND_LED_OFF, COMMAND_ARG_NONE, 0, 0 },

    { "BUZZER", "MUSIC", COMMAND_BUZZER_MUSIC, COMMAND_ARG_STRING, 0, 0},
    { "BUZZER", "ON", COMMAND_BUZZER_ON, COMMAND_ARG_NONE, 0, 0 },
    { "BUZZER", "OFF", COMMAND_BUZZER_OFF, COMMAND_ARG_NONE, 0, 0 },
    { "BUZZER", "BEEP", COMMAND_BUZZER_BEEP, COMMAND_ARG_INT, 1, 10000 },

    { "LIGHT", "READ", COMMAND_LIGHT_READ, COMMAND_ARG_NONE, 0, 0 },

    { "SEG", "DISPLAY", COMMAND_SEG_DISPLAY, COMMAND_ARG_INT, 0, 9 },
    { "SEG", "CLEAR", COMMAND_SEG_CLEAR, COMMAND_ARG_NONE, 0, 0 },
};

/**
 * Compares two command tokens case-insensitively, including NULL tokens.
 * @param left The first token.
 * @param right The second token.
 * @return true if the tokens match, false otherwise.
 */
static bool token_equals(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    return strcasecmp(left, right) == 0;
}

/**
 * Parses an integer command argument and validates its allowed range.
 * @param token The token to parse.
 * @param min_value The minimum accepted value.
 * @param max_value The maximum accepted value.
 * @param value Receives the parsed integer when parsing succeeds.
 * @return true if the token is a valid integer in range, false otherwise.
 */
static bool parse_int_arg(
    const char *token,
    int min_value,
    int max_value,
    int *value
) {
    char *endptr;
    long number;

    if (token == NULL || *token == '\0') {
        return false;
    }

    errno = 0;
    number = strtol(token, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || number < min_value || number > max_value) {
        return false;
    }

    *value = (int)number;
    return true;
}

ParsedCommand command_parse(const char *line) {
    ParsedCommand parsed = {
        .type = COMMAND_INVALID,
        .value = 0,
        .str = ""
    };
    char copy[COMMAND_LINE_MAX];
    char *saveptr = NULL;
    char *device;
    char *action;
    char *value;
    char *extra;

    if (line == NULL) {
        parsed.type = COMMAND_EMPTY;
        return parsed;
    }

    if (strlen(line) >= sizeof(copy)) {
        parsed.type = COMMAND_INVALID;
        return parsed;
    }

    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    device = strtok_r(copy, " \t\r\n", &saveptr);
    if (device == NULL) {
        parsed.type = COMMAND_EMPTY;
        return parsed;
    }

    action = strtok_r(NULL, " \t\r\n", &saveptr);
    value = strtok_r(NULL, " \t\r\n", &saveptr);
    extra = strtok_r(NULL, " \t\r\n", &saveptr);

    for (size_t i = 0; i < sizeof(command_specs) / sizeof(command_specs[0]); i++) {
        const CommandSpec *spec = &command_specs[i];

        if (!token_equals(device, spec->device) || !token_equals(action, spec->action)) {
            continue;
        }

        switch (spec->arg_type) {
        case COMMAND_ARG_NONE:
            if (value != NULL) {
                parsed.type = COMMAND_INVALID;
                return parsed;
            }

            parsed.type = spec->command;
            return parsed;
            break;

        case COMMAND_ARG_INT:
            if (extra != NULL || !parse_int_arg(value, spec->min_value, spec->max_value, &parsed.value)) {
                parsed.type = COMMAND_INVALID_VALUE;
                return parsed;
            }
            break;

        case COMMAND_ARG_STRING:
            if (value == NULL) {
                parsed.type = COMMAND_INVALID_VALUE;
                return parsed;
            }
            strncpy(parsed.str, value, sizeof(parsed.str) - 1);
            break;
        }

        parsed.str[sizeof(parsed.str) - 1] = '\0';
        parsed.type = spec->command;
        return parsed;
    }

    parsed.type = value == NULL ? COMMAND_INVALID : COMMAND_INVALID_VALUE;
    return parsed;
}

const char *command_name(Command command) {
    switch (command) {
    case COMMAND_EMPTY:
        return "EMPTY";
    case COMMAND_INVALID:
        return "INVALID";
    case COMMAND_INVALID_VALUE:
        return "INVALID_VALUE";
    case COMMAND_HELP:
        return "HELP";
    case COMMAND_STATUS:
        return "STATUS";
    case COMMAND_PLUGIN_LIST:
        return "PLUGIN_LIST";
    case COMMAND_QUIT:
        return "QUIT";
    case COMMAND_LED_ON:
        return "LED_ON";
    case COMMAND_LED_ON_BRIGHT:
        return "LED_ON_BRIGHT";
    case COMMAND_LED_OFF:
        return "LED_OFF";
    case COMMAND_BUZZER_MUSIC:
        return "BUZZER_MUSIC";
    case COMMAND_BUZZER_ON:
        return "BUZZER_ON";
    case COMMAND_BUZZER_OFF:
        return "BUZZER_OFF";
    case COMMAND_BUZZER_BEEP:
        return "BUZZER_BEEP";
    case COMMAND_LIGHT_READ:
        return "LIGHT_READ";
    case COMMAND_SEG_DISPLAY:
        return "SEG_DISPLAY";
    case COMMAND_SEG_CLEAR:
        return "SEG_CLEAR";
    }

    return "UNKNOWN";
}
