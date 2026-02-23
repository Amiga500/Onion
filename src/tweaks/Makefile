INCLUDE_SHMVAR=1
INCLUDE_CJSON=1
include ../common/config.mk

TARGET = tweaks
CFLAGS := $(CFLAGS) -D_DEFAULT_SOURCE -DHAS_AUDIO
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -lSDL_rotozoom -pthread -lkbinput

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
