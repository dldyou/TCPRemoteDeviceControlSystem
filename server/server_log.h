#pragma once

/**
 * Logs a client command and its server response with timestamp and peer address.
 * @param client_fd The client socket descriptor.
 * @param request The raw request received from the client.
 * @param response The response sent back to the client.
 */
void log_client_command(
    int client_fd,
    const char *request,
    const char *response
);
