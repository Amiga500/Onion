INCLUDE_SHMVAR=1
INCLUDE_CJSON=1
include ../common/config.mk

TARGET = prompt
CFLAGS := $(CFLAGS) -DHAS_AUDIO
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -lSDL_rotozoom -pthread

# Check if SDL headers are available before attempting to build
SDL_AVAILABLE := $(shell $(CC) $(CFLAGS) -include SDL/SDL.h -x c -c /dev/null -o /dev/null 2>/dev/null && echo yes)

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
