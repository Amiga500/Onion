TEST = 1
INCLUDE_UTILS = 0
CFILES := ../src/infoPanel/imagesCache.c ../src/common/utils/log.c ../src/common/utils/file.c ../src/common/utils/str.c
include ../src/common/config.mk

TARGET = test
LDFLAGS := $(LDFLAGS) -L../lib -s -lSDL_image -lSDL -lSDL_rotozoom -lgtest -lgtest_main -lpthread

STR_TEST_TARGET = str_test
STR_TEST_OFILES = test_str.o ../src/common/utils/str.o

include ../src/common/commands.mk
include ../src/common/recipes.mk

$(STR_TEST_TARGET): $(STR_TEST_OFILES)
	@mkdir -p "$(BUILD_DIR)"
	@$(CC) $(STR_TEST_OFILES) -o "$(BUILD_DIR)/$(STR_TEST_TARGET)"

str-test: $(STR_TEST_TARGET)
