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
AS 		= $(CROSS_COMPILE)gcc
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

CFLAGS := -I../../include -I../common -DPLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z) -DONION_VERSION="\"$(VERSION)\"" -Wall -O3

ifeq ($(DEBUG),1)
CFLAGS := $(CFLAGS) -DLOG_DEBUG -g3
endif

ifeq ($(TEST),1)
CFLAGS := $(CFLAGS) -I../include -I../src/common -I$(GTEST_INCLUDE_DIR)
endif

ifeq ($(SANITIZE),1)
CFILES := $(CFILES) ../common/utils/asan.c
CFLAGS := $(CFLAGS) -fno-omit-frame-pointer -fsanitize=address -static-libasan
LDFLAGS := -fsanitize=address -static-libasan $(LDFLAGS)
endif

CXXFLAGS := $(CFLAGS)
LDFLAGS := $(LDFLAGS) -L../../lib -L/usr/local/lib

ifeq ($(PLATFORM),miyoomini)
CFLAGS := $(CFLAGS) -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -Wl,-rpath=$(LIB) -ffunction-sections -fdata-sections
LDFLAGS := $(LDFLAGS) -Wl,--gc-sections

# Enable NEON assembly optimizations (optional, define USE_NEON_ASM=1 to use)
# When enabled, the assembly file is compiled and linked, providing 15-30% 
# additional performance over C intrinsics for pixel operations.
ifdef USE_NEON_ASM
CFLAGS := $(CFLAGS) -DUSE_NEON_ASM=1
# Assembly flags for NEON on Cortex-A7 (used with gcc as assembler driver)
ASFLAGS := -marm -march=armv7ve -mfpu=neon-vfpv4 -mfloat-abi=hard
NEON_ASM_OBJ := ../common/utils/neon_asm.o
OFILES := $(OFILES) $(NEON_ASM_OBJ)

# Rule for assembling the NEON assembly file
$(NEON_ASM_OBJ): ../common/utils/neon_asm.S
	$(AS) $(ASFLAGS) -c $< -o $@
endif

ifdef INCLUDE_SHMVAR
LDFLAGS := $(LDFLAGS) -lshmvar
endif

endif
