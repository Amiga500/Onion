#include "system/axp.h"

int main(int argc, char **argv)
{
    unsigned int reg_address, i, j;
    int w_val, val, ret;

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

    if (sscanf(argv[1], "%x", &reg_address) != 1 || reg_address > 0xff) {
        fprintf(stderr, "Error on register %s\n", argv[1]);
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
        // A value that is not a byte is refused (it was reported and
        // written anyway)
        if (sscanf(argv[2], "%x", &w_val) != 1 || (w_val & ~0xff) != 0) {
            fprintf(stderr, "Error on written value %s\n", argv[2]);
            return 1;
        }
        if (argv[2][0] == '+') {
            // "+bits": read, OR, write. A failed read is not written back
            // as 0xFF (-1 ORed): every bit of the register on.
            val = axp_read(reg_address);
            if (val < 0) {
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
