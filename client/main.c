#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <poll.h>
#include <signal.h>
#include <strings.h>

#include "app_config.h"

#define _GNU_SOURCE

#define COLOR_RESET "\033[0m"
#define COLOR_DIM "\033[2m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_CYAN "\033[36m"
#define COLOR_YELLOW "\033[33m"
#define PROMPT COLOR_CYAN "device> " COLOR_RESET

static char buffer[BUFFER_SIZE] = "";
static volatile sig_atomic_t client_running = 1;

/**
 * Signal handler for SIGINT to allow graceful shutdown of the client.
 */
static void handle_signum(int signal_number) {
    (void)signal_number;
    if (signal_number == SIGINT) {
        client_running = 0;
    }
}

/**
 * Only quit on SIGINT (Ctrl+C) and ignore other signals to prevent accidental termination of the client. The server can be stopped by sending a QUIT command or by closing the connection.
 */
static void set_signal_handler(void) {
    struct sigaction sa;
    sa.sa_handler = handle_signum;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}

/**
 * Trims trailing newline characters from the input string.
 * @param str The string to trim. This string is modified in place.
 */
static void trim_newline(char *str) {
    size_t length;

    if (str == NULL) {
        return;
    }

    length = strlen(str);
    while (length > 0 && (str[length - 1] == '\n' || str[length - 1] == '\r')) {
        str[length - 1] = '\0';
        length -= 1;
    }
}

/**
 * Checks if the input string is blank (NULL, empty, or only whitespace).
 * @param str The string to check.
 * @return 1 if the string is blank, 0 otherwise.
 */
static int is_blank(const char *str) {
    if (str == NULL) {
        return 1;
    }

    while (*str != '\0') {
        if (*str != ' ' && *str != '\t') {
            return 0;
        }
        str += 1;
    }

    return 1;
}

/**
 * Prints the command prompt to the console.
 */
static void print_prompt(void) {
    printf(PROMPT);
    fflush(stdout);
}

/**
 * Prints the client banner with usage information.
 */
static void print_banner(void) {
    printf("\n");
    printf(COLOR_GREEN "TCP Remote Device Control Client" COLOR_RESET "\n");
    printf(COLOR_DIM "Connected commands are sent to the Raspberry Pi server." COLOR_RESET "\n");
    printf(COLOR_DIM "Local commands: .help, .clear, .quit" COLOR_RESET "\n");
    printf("\n");
}

/**
 * Prints the local help information for the client commands and server commands.
 */
static void print_local_help(void) {
    printf(COLOR_YELLOW "Local UI commands" COLOR_RESET "\n");
    printf("  .help             Show this client help\n");
    printf("  .clear            Clear the terminal\n");
    printf("  .quit             Close the client\n");
    printf("\n");
    printf(COLOR_YELLOW "Server commands" COLOR_RESET "\n");
    printf("  HELP\n");
    printf("  STATUS\n");
    printf("  PLUGIN LIST\n");
    printf("  LED ON | LED BRIGHT <0-255> | LED OFF\n");
    printf("  BUZZER ON | BUZZER OFF | BUZZER BEEP <ms> | BUZZER MUSIC <file>\n");
    printf("  LIGHT READ\n");
    printf("  SEG DISPLAY <0-9> | SEG CLEAR\n");
    printf("  QUIT\n");
}

/**
 * Clears the terminal screen and moves the cursor to the top-left corner.
 */
static void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

/**
 * Handles a local command entered by the user. Local commands are processed by the client and not sent to the server.
 * @param line The input command line to handle.
 * @param socket_fd The socket file descriptor to use for sending commands to the server if needed.
 * @return - `1` if the command was a local command and was handled
 * @return - `0` if the command should be sent to the server
 * @return - `-1` if the command is a quit command and the client should exit.
 */
static int handle_local_command(char *line, int socket_fd) {
    if (strcasecmp(line, ".help") == 0 || strcasecmp(line, "client help") == 0) {
        print_local_help();
        return 1;
    }

    if (strcasecmp(line, ".clear") == 0 || strcasecmp(line, "clear") == 0) {
        clear_screen();
        return 1;
    }

    if (strcasecmp(line, ".quit") == 0 || strcasecmp(line, ".exit") == 0 ||
        strcasecmp(line, "exit") == 0) {
        printf(COLOR_DIM "Closing client." COLOR_RESET "\n");
        return -1;
    }

    if (strcasecmp(line, "quit") == 0) {
        send(socket_fd, "QUIT\n", strlen("QUIT\n"), 0);
        printf(COLOR_DIM "Sent QUIT to server." COLOR_RESET "\n");
        return -1;
    }

    return 0;
}

/**
 * Prints the server response to the console with appropriate coloring based on the response content.
 *
 * - Lines starting with "OK" are printed in green.
 *
 * - Lines starting with "ERR" are printed in red.
 *
 * - All other lines are printed in the default color.
 *
 * If the response does not end with a newline, a newline is added after printing.
 * @param response The server response string to print.
 */
static void print_server_response(const char *response) {
    if (strncmp(response, "OK", 2) == 0) {
        printf(COLOR_GREEN "< %s" COLOR_RESET, response);
    }
    else if (strncmp(response, "ERR", 3) == 0) {
        printf(COLOR_RED "< %s" COLOR_RESET, response);
    }
    else {
        printf("< %s", response);
    }

    if (response[0] != '\0' && response[strlen(response) - 1] != '\n') {
        printf("\n");
    }
}

/**
 * Sends a command string to the server over the socket connection. A newline character is appended to the command before sending.
 * @param socket_fd The socket file descriptor to use for sending the command.
 * @param line The command string to send to the server (without newline).
 */
static void send_command(int socket_fd, const char *line) {
    char command[BUFFER_SIZE];

    snprintf(command, sizeof(command), "%s\n", line);
    if (send(socket_fd, command, strlen(command), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

/**
 * Runs the main console loop for the client, handling user input and server responses. This function uses `poll` to wait for input from both the user (stdin) and the server socket.
 *
 * - When the user enters a command, it is processed as a local command or sent to the server as appropriate.
 *
 * - When a response is received from the server, it is printed to the console with appropriate formatting.
 *
 * - The loop continues until the user issues a quit command or the server connection is closed.
 * @param socket_fd The socket file descriptor for the connection to the server.
 */
static void run_console(int socket_fd) {
    int num_bytes;
    struct pollfd poll_fds[2];

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = socket_fd;
    poll_fds[1].events = POLLIN;

    print_banner();
    print_prompt();

    while (client_running) {
        if (poll(poll_fds, 2, -1) == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            exit(EXIT_FAILURE);
        }

        if ((poll_fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            printf("\n" COLOR_RED "Server connection closed." COLOR_RESET "\n");
            break;
        }

        if ((poll_fds[1].revents & POLLIN) == POLLIN) {
            num_bytes = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
            if (num_bytes == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            if (num_bytes == 0) {
                printf("\n" COLOR_RED "Server disconnected." COLOR_RESET "\n");
                break;
            }

            buffer[num_bytes] = '\0';
            printf("\r");
            print_server_response(buffer);
            print_prompt();
            continue;
        }

        if ((poll_fds[0].revents & POLLIN) == POLLIN) {
            int local_result;

            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                if (feof(stdin)) {
                    clearerr(stdin);
                    printf("\n");
                    print_prompt();
                    continue;
                }

                if (ferror(stdin)) {
                    clearerr(stdin);
                    print_prompt();
                    continue;
                }
            }

            trim_newline(buffer);
            if (is_blank(buffer)) {
                print_prompt();
                continue;
            }

            local_result = handle_local_command(buffer, socket_fd);
            if (local_result == -1) {
                break;
            }
            if (local_result == 1) {
                print_prompt();
                continue;
            }

            printf(COLOR_DIM "> %s" COLOR_RESET "\n", buffer);
            send_command(socket_fd, buffer);
        }
    }
}

/**
 * The main entry point for the TCP Remote Device Control Client application. This function performs the following steps:
 * 1. Ignores the SIGPIPE signal to prevent the client from crashing if the server disconnects.
 * 2. Loads the application configuration from "config.ini" using the `load_app_config` function. If the configuration cannot be loaded, an error message is printed and the client exits.
 * 3. Creates a TCP socket and connects to the server using the address and port specified in the configuration. If the connection fails, an error message is printed and the client exits.
 * 4. If the connection is successful, a message is printed to the console, and
 */
int main(int argc, char *argv[]) {
    int socket_fd;
    struct sockaddr_in server_addr = { 0 };

    (void)argc;
    (void)argv;

    set_signal_handler();

    if (!load_app_config("config.ini")) {
        fprintf(stderr, "failed to load config.ini\n");
        exit(EXIT_FAILURE);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(app.port);

    if (inet_pton(AF_INET, app.addr, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connecting to %s:%d...\n", app.addr, app.port);
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf(COLOR_GREEN "Connected." COLOR_RESET "\n");
    run_console(socket_fd);

    close(socket_fd);
    return 0;
}
