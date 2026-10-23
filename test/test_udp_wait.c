/**
 * @file test_udp_wait.c
 * @brief GET_STATUS when RetroArch is slow to answer (udp.c, retroarch_cmd.c)
 *
 * The first GET_STATUS for a Neo Geo zip makes RetroArch read the whole file
 * for its CRC32 before it answers anything. The GameSwitcher waited 250 ms,
 * then probed with VERSION, which RetroArch could not answer either, so it
 * gave up: no autosave from the GameSwitcher and no rom screen PNG.
 *
 * A fake RetroArch on localhost reads one request, stays busy, then replies.
 *
 * Build and run: make -f Makefile.unit test_udp_wait
 */

#include "onion_test.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "utils/retroarch_cmd.h"
#include "utils/udp.h"

#define RA_PORT 55355

typedef struct {
    int fd;
    int busy_ms; // time spent "computing the CRC" before replying
    const char *reply;
    char request[64];
} FakeRetroArch;

static long _now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int _bind_udp(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int _port_of(int fd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(fd, (struct sockaddr *)&addr, &len);
    return ntohs(addr.sin_port);
}

// Like RetroArch's main loop: one request makes it busy, then it answers
// that one and whatever queued up meanwhile (VERSION probes included) until
// it has been idle for a moment. Replies to closed sockets are lost.
static void *_fake_retroarch(void *arg)
{
    FakeRetroArch *ra = arg;
    bool first = true;
    for (;;) {
        char buf[64];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(ra->fd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&from, &from_len);
        if (n < 0)
            return NULL;
        buf[n] = '\0';
        if (first) {
            strcpy(ra->request, buf);
            usleep(ra->busy_ms * 1000);
            struct timeval idle = {.tv_sec = 0, .tv_usec = 300000};
            setsockopt(ra->fd, SOL_SOCKET, SO_RCVTIMEO, &idle, sizeof(idle));
            first = false;
        }
        const char *reply = strcmp(buf, "VERSION") == 0 ? "1.15.0" : ra->reply;
        sendto(ra->fd, reply, strlen(reply), 0, (struct sockaddr *)&from, from_len);
    }
}

static bool _stop_waiting(void *ctx)
{
    int *calls = ctx;
    (*calls)++;
    return false;
}

TEST(slow_reply_is_received)
{
    FakeRetroArch ra = {.fd = _bind_udp(0), .busy_ms = 2500, .reply = "GET_STATUS PAUSED"};
    ASSERT_TRUE(ra.fd >= 0);
    pthread_t t;
    pthread_create(&t, NULL, _fake_retroarch, &ra);

    char response[64];
    int rc = udp_send_receive_wait("127.0.0.1", _port_of(ra.fd), "GET_STATUS",
                                   response, sizeof(response), 15000, NULL, NULL);
    pthread_join(t, NULL);
    close(ra.fd);

    ASSERT_EQ(rc, 0);
    ASSERT_STREQ(ra.request, "GET_STATUS");
    ASSERT_STREQ(response, "GET_STATUS PAUSED");
}

// What beta 9 did: 250 ms without probe, then the probed path. RetroArch,
// still busy with the CRC, answers no VERSION in time.
TEST(old_short_wait_then_probe_gives_up)
{
    FakeRetroArch ra = {.fd = _bind_udp(0), .busy_ms = 2500, .reply = "GET_STATUS PAUSED"};
    ASSERT_TRUE(ra.fd >= 0);
    int port = _port_of(ra.fd);
    pthread_t t;
    pthread_create(&t, NULL, _fake_retroarch, &ra);

    char response[64];
    int rc = udp_send_receive_no_probe("127.0.0.1", port, "GET_STATUS",
                                       response, sizeof(response));
    if (rc != 0)
        rc = udp_send_receive("127.0.0.1", port, "GET_STATUS", response, sizeof(response));
    pthread_join(t, NULL);
    close(ra.fd);

    ASSERT_EQ(rc, -1);
}

TEST(fast_reply_is_quick)
{
    FakeRetroArch ra = {.fd = _bind_udp(0), .busy_ms = 0, .reply = "GET_STATUS PLAYING x"};
    ASSERT_TRUE(ra.fd >= 0);
    pthread_t t;
    pthread_create(&t, NULL, _fake_retroarch, &ra);

    char response[64];
    long start = _now_ms();
    int rc = udp_send_receive_wait("127.0.0.1", _port_of(ra.fd), "GET_STATUS",
                                   response, sizeof(response), 15000, NULL, NULL);
    long elapsed = _now_ms() - start;
    pthread_join(t, NULL);
    close(ra.fd);

    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(elapsed < 200);
}

TEST(no_listener_times_out)
{
    int fd = _bind_udp(0); // bound, never read: a RetroArch that never answers
    ASSERT_TRUE(fd >= 0);
    char response[64];
    long start = _now_ms();
    int rc = udp_send_receive_wait("127.0.0.1", _port_of(fd), "GET_STATUS",
                                   response, sizeof(response), 600, NULL, NULL);
    long elapsed = _now_ms() - start;
    close(fd);

    ASSERT_EQ(rc, -1);
    ASSERT_TRUE(elapsed >= 500);
    ASSERT_TRUE(elapsed < 1500);
}

// RetroArch gone: the GameSwitcher must not sit out the whole timeout.
TEST(keep_waiting_false_stops_early)
{
    int fd = _bind_udp(0);
    ASSERT_TRUE(fd >= 0);
    char response[64];
    int calls = 0;
    long start = _now_ms();
    int rc = udp_send_receive_wait("127.0.0.1", _port_of(fd), "GET_STATUS",
                                   response, sizeof(response), 15000, _stop_waiting, &calls);
    long elapsed = _now_ms() - start;
    close(fd);

    ASSERT_EQ(rc, -1);
    ASSERT_EQ(calls, 1);
    ASSERT_TRUE(elapsed < 1000);
}

TEST(bad_arguments)
{
    char response[64];
    ASSERT_EQ(udp_send_receive_wait("127.0.0.1", 9, "X", NULL, 64, 100, NULL, NULL), -1);
    ASSERT_EQ(udp_send_receive_wait("127.0.0.1", 9, "X", response, 1, 100, NULL, NULL), -1);
    ASSERT_EQ(udp_send_receive_wait("127.0.0.1", 9, "X", response, sizeof(response), 0, NULL, NULL), -1);
}

// Through retroarch_cmd on RetroArch's own port (skipped if it is taken).
TEST(get_status_wait_parses_neo_geo_reply)
{
    FakeRetroArch ra = {.fd = _bind_udp(RA_PORT), .busy_ms = 1500, .reply = "GET_STATUS PAUSED fb_alpha,mslugx,crc32=3a5a3b1f\n"};
    if (ra.fd < 0) {
        printf("  (port %d in use, skipped)\n", RA_PORT);
        return;
    }
    pthread_t t;
    pthread_create(&t, NULL, _fake_retroarch, &ra);

    RetroArchStatus_s status;
    int rc = retroarch_getStatusWait(&status, 15000, NULL, NULL);
    pthread_join(t, NULL);
    close(ra.fd);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(status.state, RETROARCH_STATE_PAUSED);
    ASSERT_STREQ(status.content_info, "fb_alpha,mslugx,crc32=3a5a3b1f");
}

int main(void)
{
    printf("\n=== GET_STATUS slow reply (udp_send_receive_wait) ===\n\n");

    RUN_TEST(slow_reply_is_received);
    RUN_TEST(old_short_wait_then_probe_gives_up);
    RUN_TEST(fast_reply_is_quick);
    RUN_TEST(no_listener_times_out);
    RUN_TEST(keep_waiting_false_stops_early);
    RUN_TEST(bad_arguments);
    RUN_TEST(get_status_wait_parses_neo_geo_reply);

    TEST_REPORT();
    return test_failures;
}
