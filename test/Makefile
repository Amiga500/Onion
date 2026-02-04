TEST = 1
INCLUDE_UTILS = 0
CFILES := ../src/infoPanel/imagesCache.c ../src/common/utils/log.c ../src/common/utils/file.c ../src/common/utils/str.c
CPPFILES := test_infoPanel.cpp test_neon_simd.cpp
include ../src/common/config.mk

TARGET = test
LDFLAGS := $(LDFLAGS) -L../lib -s -lSDL_image -lSDL -lSDL_rotozoom -lgtest -lgtest_main -lpthread

include ../src/common/commands.mk
include ../src/common/recipes.mk