#include <ctype.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KEY_ESC 1

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s program_name\n", argv[0]);
        return 1;
    }

    /* Validate argv[1]: only allow characters safe in a shell argument
     * (alphanumerics, dash, underscore, dot, slash) to prevent injection. */
    for (const char *p = argv[1]; *p; p++) {
        if (!isalnum((unsigned char)*p) && strchr("-_./", *p) == NULL) {
            fprintf(stderr, "Invalid program name: unsafe characters\n");
            return 1;
        }
    }

    /* "pkill -9 -f -- " (15) + argv[1] (up to NAME_MAX=255) + NUL = 271 B max */
    char command[280];
    snprintf(command, sizeof(command), "pkill -9 -f -- %s", argv[1]);

    int input_fd = open("/dev/input/event0", O_RDONLY);
    if (input_fd < 0) {
        fprintf(stderr, "Failed to open /dev/input/event0\n");
        return 1;
    }
    struct input_event event;

    while (read(input_fd, &event, sizeof(event)) == sizeof(event)) {
        if (event.type == EV_KEY && event.value == 1) {
            if (event.code == KEY_ESC) {
                system(command);
                break;
            }
        }
    }

    close(input_fd);
    return 0;
}
