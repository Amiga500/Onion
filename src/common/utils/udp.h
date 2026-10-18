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
#endif // UTILS_UDP_H__
