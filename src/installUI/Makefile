INCLUDE_CJSON=1
CFILES := \
	../common/utils/imageCache.c
include ../common/config.mk

TARGET = installUI
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -pthread

# Check if SDL headers are available before attempting to build
SDL_AVAILABLE := $(shell echo '#include <SDL/SDL.h>' | $(CC) $(CFLAGS) -x c -c - -o /dev/null 2>/dev/null && echo yes)

ifneq ($(SDL_AVAILABLE),yes)
.PHONY: all build clean dev asan
all build dev asan:
	@echo "-- SDL headers not found, skipping $(TARGET) build (install libsdl1.2-dev)"
clean:
	@true
else
include ../common/commands.mk
include ../common/recipes.mk
endif
