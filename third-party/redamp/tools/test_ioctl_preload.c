/* Test the actual LD_PRELOAD hook rather than an isolated ioctl imitation. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
int main(void) {
    int p[2], n = -1, nonblock = 1;
    if (pipe(p) != 0 || write(p[1], "hello", 5) != 5) return 1;
    if (ioctl(p[0], FIONREAD, &n) != 0 || n != 5) {
        fprintf(stderr, "FIONREAD did not forward its pointer: n=%d\n", n); return 2;
    }
    if (ioctl(p[0], FIONBIO, &nonblock) != 0) return 3;
    if (fcntl(p[0], F_GETFL) < 0 || !(fcntl(p[0], F_GETFL) & O_NONBLOCK)) return 4;
    if (ioctl(p[0], FIOCLEX) != 0) return 5; /* genuinely argument-free */
    if (!(fcntl(p[0], F_GETFD) & FD_CLOEXEC)) return 6;
    close(p[0]); close(p[1]);
    puts("PASS: preloaded ioctl(FIONREAD/FIONBIO/FIOCLEX)");
    return 0;
}
