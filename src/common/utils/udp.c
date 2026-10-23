#include "udp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static const int PING_TIMEOUT_MS = 100;
static const int MAX_RETRIES = 3;
static const int RETRY_DELAY_MS = 500;
static const int RECV_TIMEOUT_MS = 60000;
#define UDP_WAIT_SLICE_MS 250

static int _init_socket(const char *ipAddress, int port, int *socket_fd, struct sockaddr_in *server_address)
{
    *socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*socket_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    memset(server_address, 0, sizeof(*server_address));
    server_address->sin_family = AF_INET;
    server_address->sin_port = htons(port);
    if (inet_pton(AF_INET, ipAddress, &server_address->sin_addr) <= 0) {
        perror("Invalid IP address");
        close(*socket_fd);
        return -1;
    }

    return 0;
}

static int _ping_socket(int socket_fd, struct sockaddr_in *server_address)
{
    const char *ping_message = "VERSION";
    char response[32];
    struct timeval timeout;
    timeout.tv_sec = PING_TIMEOUT_MS / 1000;
    timeout.tv_usec = (PING_TIMEOUT_MS % 1000) * 1000;

    // Set receive timeout
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket receive timeout");
        return -1;
    }

    // Send ping message
    if (sendto(socket_fd, ping_message, strlen(ping_message), 0, (struct sockaddr *)server_address, sizeof(*server_address)) == -1) {
        perror("Failed to send ping");
        return -1;
    }

    // Wait for response
    ssize_t bytes_received = recvfrom(socket_fd, response, sizeof(response), 0, NULL, NULL);
    if (bytes_received == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            fprintf(stderr, "Ping timeout\n");
        }
        else {
            perror("Failed to receive ping response");
        }
        return -1;
    }

    return 0;
}

static int _udp_send(const char *ipAddress, int port, const char *message)
{
    int socket_fd;
    struct sockaddr_in server_address;

    if (_init_socket(ipAddress, port, &socket_fd, &server_address) == -1) {
        return -1;
    }

    if (_ping_socket(socket_fd, &server_address) == -1) {
        close(socket_fd);
        return -1;
    }

    if (sendto(socket_fd, message, strlen(message), 0, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to send data");
        close(socket_fd);
        return -1;
    }

    close(socket_fd);
    return 0;
}

static int _udp_send_receive(const char *ipAddress, int port, const char *message, char *response, size_t response_size)
{
    int socket_fd;
    struct sockaddr_in server_address;

    if (_init_socket(ipAddress, port, &socket_fd, &server_address) == -1) {
        return -1;
    }

    if (_ping_socket(socket_fd, &server_address) == -1) {
        close(socket_fd);
        return -1;
    }

    if (sendto(socket_fd, message, strlen(message), 0, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to send data");
        close(socket_fd);
        return -1;
    }

    // Set a longer timeout for receiving the response
    struct timeval timeout;
    timeout.tv_sec = RECV_TIMEOUT_MS / 1000;
    timeout.tv_usec = (RECV_TIMEOUT_MS % 1000) * 1000;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket receive timeout");
        close(socket_fd);
        return -1;
    }

    ssize_t bytes_received = recvfrom(socket_fd, response, response_size, 0, NULL, NULL);
    if (bytes_received == -1) {
        perror("Failed to receive data");
        close(socket_fd);
        return -1;
    }

    response[bytes_received] = '\0'; // Null-terminate the received string

    close(socket_fd);
    return 0;
}

int udp_send(const char *ipAddress, int port, const char *message)
{
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (_udp_send(ipAddress, port, message) == 0) {
            return 0; // Success
        }
        usleep(RETRY_DELAY_MS * 1000); // Delay before retrying
    }
    return -1; // Failed after retries
}

int udp_send_receive(const char *ipAddress, int port, const char *message, char *response, size_t response_size)
{
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (_udp_send_receive(ipAddress, port, message, response, response_size) == 0) {
            return 0; // Success
        }
        usleep(RETRY_DELAY_MS * 1000); // Delay before retrying
    }
    return -1; // Failed after retries
}

// Avoid an extra VERSION round trip for commands sent after initial handshake.
// GET_STATUS gets a short bounded timeout; callers can retry the normal path.
int udp_send_no_probe(const char *ipAddress, int port, const char *message)
{
    int fd;
    struct sockaddr_in addr;
    if (_init_socket(ipAddress, port, &fd, &addr) != 0)
        return -1;
    int rc = sendto(fd, message, strlen(message), 0,
                    (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
    return rc < 0 ? -1 : 0;
}

// Send one request and wait up to timeout_ms for its reply on the same
// socket, in short slices. Between slices keep_waiting (when given) can stop
// the wait early, e.g. once the receiving process is gone.
int udp_send_receive_wait(const char *ipAddress, int port, const char *message,
                          char *response, size_t response_size, int timeout_ms,
                          udp_keep_waiting_fn keep_waiting, void *ctx)
{
    if (response == NULL || response_size < 2 || timeout_ms <= 0)
        return -1;
    int fd;
    struct sockaddr_in addr;
    if (_init_socket(ipAddress, port, &fd, &addr) != 0)
        return -1;
    int slice_ms = timeout_ms < UDP_WAIT_SLICE_MS ? timeout_ms : UDP_WAIT_SLICE_MS;
    struct timeval timeout = {.tv_sec = slice_ms / 1000,
                              .tv_usec = (slice_ms % 1000) * 1000};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        close(fd);
        return -1;
    }
    if (sendto(fd, message, strlen(message), 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    for (int waited_ms = 0;;) {
        ssize_t n = recvfrom(fd, response, response_size - 1, 0, NULL, NULL);
        if (n >= 0) {
            close(fd);
            response[n] = '\0';
            return 0;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            break;
        waited_ms += slice_ms;
        if (waited_ms >= timeout_ms)
            break;
        if (keep_waiting != NULL && !keep_waiting(ctx))
            break;
    }
    close(fd);
    return -1;
}

int udp_send_receive_no_probe(const char *ipAddress, int port, const char *message,
                              char *response, size_t response_size)
{
    return udp_send_receive_wait(ipAddress, port, message, response, response_size,
                                 UDP_WAIT_SLICE_MS, NULL, NULL);
}
