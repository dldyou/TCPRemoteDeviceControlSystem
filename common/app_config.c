#include "app_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

app_config app = {
    .port = 60000,
    .addr = "127.0.0.1"
};

/**
 * Trims leading and trailing whitespace from a string in place.
 * @param str The string to trim.
 * @return A pointer to the first non-whitespace character in the same buffer.
 */
static char *trim(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) {
        str += 1;
    }

    if (*str == 0) {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end -= 1;
    }
    end[1] = '\0';

    return str;
}

bool load_app_config(const char *ini_file_path) {
    FILE *file;
    if ((file = fopen(ini_file_path, "r")) == NULL) {
        perror("fopen");
        return false;
    }

    char line[256];
    char current_section[64] = { 0 };

    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim(line);

        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#' || trimmed_line[0] == ';') {
            continue;
        }

        if (trimmed_line[0] == '[' && trimmed_line[strlen(trimmed_line) - 1] == ']') {
            strncpy(current_section, trimmed_line + 1, strlen(trimmed_line) - 2);
            current_section[strlen(trimmed_line) - 2] = '\0';
            trim(current_section);
            continue;
        }

        char* delimiter_index = strchr(trimmed_line, '=');
        if (delimiter_index != NULL) {
            *delimiter_index = '\0';

            char* key = trim(trimmed_line);
            char* value = trim(delimiter_index + 1);

            if (strcmp(current_section, "Server") == 0) {
                if (strcmp(key, "port") == 0) {
                    app.port = atoi(value);
                }
                else if (strcmp(key, "addr") == 0) {
                    strncpy(app.addr, value, MAX_ADDR_LEN - 1);
                    app.addr[MAX_ADDR_LEN - 1] = '\0';
                }
            }
        }
    }

    fclose(file);
    return true;
}
