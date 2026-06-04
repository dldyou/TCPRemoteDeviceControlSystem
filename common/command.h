#pragma once

typedef enum {
    COMMAND_EMPTY = 0,
    COMMAND_INVALID,
    COMMAND_INVALID_VALUE,

    COMMAND_HELP,
    COMMAND_STATUS,
    COMMAND_PLUGIN_LIST,
    COMMAND_QUIT,

    COMMAND_LED_ON,
    COMMAND_LED_ON_BRIGHT,
    COMMAND_LED_OFF,

    COMMAND_BUZZER_MUSIC,
    COMMAND_BUZZER_ON,
    COMMAND_BUZZER_OFF,
    COMMAND_BUZZER_BEEP,

    COMMAND_LIGHT_READ,

    COMMAND_SEG_DISPLAY,
    COMMAND_SEG_CLEAR
} Command;

typedef struct {
    Command type;
    int value;
    char str[128];
} ParsedCommand;

/**
 * Parses a raw command line into a typed command and optional argument.
 * @param line The command line received from the client.
 * @return The parsed command descriptor.
 */
ParsedCommand command_parse(const char *line);

/**
 * Converts a command enum value to a readable name.
 * @param command The command enum value.
 * @return A static string name for the command.
 */
const char *command_name(Command command);
