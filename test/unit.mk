# Host-only unit tests (no SDL, no gtest, no toolchain).
# Invoked from the repo root: make unit-test

UNIT_BUILD ?= $(ROOT_DIR)/build_unit
CC ?= gcc
UNIT_CFLAGS = -Wall -Wextra -Wno-unused-parameter -I$(ROOT_DIR)/src/common -I$(ROOT_DIR)/test -DPLATFORM_LINUX -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

UNIT_STR_SRC = $(ROOT_DIR)/test/test_str.c $(ROOT_DIR)/src/common/utils/str.c
UNIT_FILE_SRC = $(ROOT_DIR)/test/test_file.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c \
	$(ROOT_DIR)/src/common/utils/log.c

.PHONY: unit-test unit-test-clean

unit-test:
	@$(makedir) $(UNIT_BUILD)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_str $(UNIT_STR_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_file $(UNIT_FILE_SRC)
	@echo
	@$(UNIT_BUILD)/test_str
	@$(UNIT_BUILD)/test_file
	@echo
	@echo "unit-test: all host suites passed"

unit-test-clean:
	@rm -rf $(UNIT_BUILD)
