#ifndef UTILS_UDP_H__
#define UTILS_UDP_H__

#include <stdbool.h>
#include <stddef.h>

int udp_send(const char *ipAddress, int port, const char *message);
int udp_send_receive(const char *ipAddress, int port, const char *message, char *response, size_t response_size);

// Use only after a successful probed command on the same RetroArch session.
int udp_send_no_probe(const char *ipAddress, int port, const char *message);
int udp_send_receive_no_probe(const char *ipAddress, int port, const char *message,
                              char *response, size_t response_size);

// Return false to stop waiting for a reply.
typedef bool (*udp_keep_waiting_fn)(void *ctx);

// No probe: one request, then wait up to timeout_ms for its reply on the same
// socket. For a listener known to be alive that may be slow to answer.
int udp_send_receive_wait(const char *ipAddress, int port, const char *message,
                          char *response, size_t response_size, int timeout_ms,
                          udp_keep_waiting_fn keep_waiting, void *ctx);
#endif // UTILS_UDP_H__
