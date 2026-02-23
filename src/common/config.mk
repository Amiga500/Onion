ifeq (,$(BUILD_DIR))
BUILD_DIR=$(shell pwd -P)
endif

ifeq (,$(VERSION))
VERSION="4.x.x-dev-test"
endif

PLATFORM ?= $(UNION_PLATFORM)
ifeq (,$(PLATFORM))
PLATFORM=linux
endif

LIB = /mnt/SDCARD/.tmp_update/lib

CC 		= $(CROSS_COMPILE)gcc
CXX 	= $(CROSS_COMPILE)g++
STRIP 	= $(CROSS_COMPILE)strip

SOURCES := $(SOURCES) .
ifeq ($(INCLUDE_CJSON),1)
SOURCES := $(SOURCES) ../../include/cjson
endif
ifneq ($(INCLUDE_UTILS),0)
CFILES := $(CFILES) \
	../common/utils/str.c \
	../common/utils/log.c \
	../common/utils/file.c
endif
CFILES := $(CFILES) $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
CPPFILES := $(CPPFILES) $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))
OFILES = $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

CFLAGS := -I../../include -I../common -DPLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z) -DONION_VERSION="\"$(VERSION)\"" -Wall

ifeq ($(DEBUG),1)
CFLAGS := $(CFLAGS) -DLOG_DEBUG -g3
else
CFLAGS := $(CFLAGS) -O2 -ffunction-sections -fdata-sections
LDFLAGS := $(LDFLAGS) -Wl,--gc-sections
endif

ifeq ($(TEST),1)
CFLAGS := $(CFLAGS) -I../include -I../src/common -I$(GTEST_INCLUDE_DIR)
endif

ifeq ($(PERF),1)
CFLAGS := $(CFLAGS) -DPERF_ENABLED
endif

ifeq ($(SANITIZE),1)
CFILES := $(CFILES) ../common/utils/asan.c
CFLAGS := $(CFLAGS) -fno-omit-frame-pointer -fsanitize=address -static-libasan
LDFLAGS := -fsanitize=address -static-libasan $(LDFLAGS)
endif

CXXFLAGS := $(CFLAGS)
LDFLAGS := $(LDFLAGS) -L../../lib -L/usr/local/lib

ifeq ($(PLATFORM),miyoomini)
CFLAGS := $(CFLAGS) -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -Wl,-rpath=$(LIB)

ifdef INCLUDE_SHMVAR
LDFLAGS := $(LDFLAGS) -lshmvar
endif

endif

# Detect SDL include path (for Linux native and cross-compilation builds)
SDL_SYSROOT := $(shell $(CC) -print-sysroot 2>/dev/null)
SDL_CFLAGS := $(shell $(CROSS_COMPILE)sdl-config --cflags 2>/dev/null || pkg-config --cflags sdl 2>/dev/null || if [ -d "$(SDL_SYSROOT)/usr/include/SDL" ]; then echo "-I$(SDL_SYSROOT)/usr/include"; fi)
ifneq ($(SDL_CFLAGS),)
CFLAGS := $(CFLAGS) $(SDL_CFLAGS)
CXXFLAGS := $(CXXFLAGS) $(SDL_CFLAGS)
endif
