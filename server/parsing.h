#pragma once

/**
 * Executes a client command in place and replaces the buffer with a response.
 * @param str The command buffer. The response is written into the same buffer.
 */
void exec_command(char *str);
