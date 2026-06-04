#include "socket.h"

#include "app_config.h"
#include "parsing.h"
#include "server_log.h"

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

static int client_fds[MAX_CLIENTS];
static volatile int running = 1;

/**
 * Handles the server shutdown signal by stopping the service loop.
 * @param signal_number The received signal number.
 */
static void signal_handler(int signal_number) {
    if (signal_number == SIGTERM) {
        running = 0;
    }
}

void init_clients(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }
}

/**
 * Adds a connected client file descriptor to the client table.
 * @param client_fd The connected client socket descriptor.
 */
static void add_client(int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == -1) {
            client_fds[i] = client_fd;
            return;
        }
    }
}

/**
 * Removes a client file descriptor from the client table.
 * @param client_fd The client socket descriptor to remove.
 */
static void remove_client(int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == client_fd) {
            client_fds[i] = -1;
            return;
        }
    }
}

/**
 * Sends a response message to a specific connected client.
 * @param dest_fd The destination client socket descriptor.
 * @param str The response text to send.
 */
static void send_message(int dest_fd, char *str) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] != -1 && client_fds[i] == dest_fd) {
            if (send(client_fds[i], str, strlen(str), 0) == -1) {
                perror("send");
            }
            return;
        }
    }
}

int socket_init(void) {
    int server_fd;
    struct sockaddr_in server_addr = { 0 };
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, app.backlog) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

void service(int server_fd) {
    char buffer[BUFFER_SIZE];
    char request[BUFFER_SIZE];
    int event_count, num_bytes;
    int epoll_fd;
    struct epoll_event event;
    struct epoll_event events[MAX_CLIENTS];
    socklen_t sockaddr_len = sizeof(struct sockaddr_in);

    if ((epoll_fd = epoll_create(MAX_CLIENTS)) == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    signal(SIGTERM, signal_handler);

    while (running) {
        if ((event_count = epoll_wait(epoll_fd, events, MAX_CLIENTS, -1)) == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < event_count; i++) {
            const int event_fd = events[i].data.fd;

            if (event_fd == server_fd) {
                struct sockaddr_in client_addr = { 0 };
                int client_fd;

                if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sockaddr_len)) == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                printf("Client Connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl");
                    close(client_fd);
                    continue;
                }
                add_client(client_fd);
            }
            else {
                int client_fd = event_fd;

                if ((events[i].events & EPOLLIN) == EPOLLIN) {
                    if ((num_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) == -1) {
                        perror("recv");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        remove_client(client_fd);
                        close(client_fd);
                        continue;
                    }

                    if (num_bytes == 0) {
                        printf("Client disconnected: fd=%d\n", client_fd);

                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        remove_client(client_fd);
                        close(client_fd);
                        continue;
                    }

                    buffer[num_bytes] = '\0';
                    strncpy(request, buffer, sizeof(request) - 1);
                    request[sizeof(request) - 1] = '\0';
                    // command parsing
                    exec_command(buffer);
                    log_client_command(client_fd, request, buffer);
                    send_message(client_fd, buffer);
                }
            }
        }
    }
}
