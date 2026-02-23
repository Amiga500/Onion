INCLUDE_UTILS=0
include ../common/config.mk

TARGET = jpg2png
LDFLAGS := $(LDFLAGS) -lmi_sys -lmi_gfx -lpng -Wl,-Bstatic -ljpeg -Wl,-Bdynamic

# Check if jpeg headers are available before attempting to build
JPEG_AVAILABLE := $(shell $(CC) $(CFLAGS) -include jpeglib.h -x c -c /dev/null -o /dev/null 2>/dev/null && echo yes)

ifneq ($(JPEG_AVAILABLE),yes)
.PHONY: all build clean dev asan
all build dev asan:
	@echo "-- jpeg headers not found, skipping $(TARGET) build (install libjpeg-dev)"
clean:
	@true
else
include ../common/commands.mk
include ../common/recipes.mk
endif
