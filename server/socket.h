#pragma once

/**
 * Initializes the server-side client descriptor table.
 */
void init_clients(void);

/**
 * Creates, binds, and listens on the TCP server socket.
 * @return The listening server socket descriptor.
 */
int socket_init(void);

/**
 * Runs the epoll-based server event loop.
 * @param server_fd The listening server socket descriptor.
 */
void service(int server_fd);
