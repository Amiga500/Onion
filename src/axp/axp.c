#include "system/axp.h"

int main(int argc, char **argv)
{
    unsigned int reg_address, w_val, i, j;
    int val, ret;

    if (argc < 2) {
        printf("Usage: %s reg_addr [+][write_val]\n\nRegisters:\n", argv[0]);
        for (i = 0; i < 0x100; i += 0x10) {
            printf("%02X :", i);
            for (j = i; j < i + 0x10; j++) {
                val = axp_read(j);
                if (val < 0)
                    printf(" --");
                else
                    printf(" %02X", val);
            }
            printf("\n");
        }
        return 0;
    }

    // Every number checked before anything is written to the power chip: an
    // address or a value that did not parse was left uninitialized, and
    // whatever it held was used as the register or the value.
    if (sscanf(argv[1], "%x", &reg_address) != 1 || reg_address > 0xff) {
        fprintf(stderr, "Invalid register address %s\n", argv[1]);
        return 1;
    }

    if (argc == 2) {
        val = axp_read(reg_address);
        if (val < 0) {
            fprintf(stderr, "axp read error: %d %s\n", val, strerror(errno));
            return 1;
        }
        printf("Read %s-%x reg %x, read value:%x\n", AXPDEV, AXPID, reg_address,
               val);
    }
    else {
        const char *w_str = argv[2][0] == '+' ? argv[2] + 1 : argv[2];
        // A value over 0xff was only reported, then written cut to 8 bits.
        if (sscanf(w_str, "%x", &w_val) != 1 || w_val > 0xff) {
            fprintf(stderr, "Error on written value %s\n", argv[2]);
            return 1;
        }
        if (argv[2][0] == '+') {
            val = axp_read(reg_address);
            if (val < 0) { // -1 | bits was written back as 0xFF
                fprintf(stderr, "axp read error: %d %s\n", val, strerror(errno));
                return 1;
            }
            val |= w_val;
            printf("Bit ");
        }
        else
            val = w_val;
        ret = axp_write(reg_address, val);
        if (ret < 0) {
            fprintf(stderr, "axp write error: %d %s\n", ret, strerror(errno));
            return 1;
        }
        printf("Write %s-%x reg %x, write value:%x\n", AXPDEV, AXPID,
               reg_address, val);
    }

    return 0;
}
