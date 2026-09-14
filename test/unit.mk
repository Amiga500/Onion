# Host-only unit tests (no SDL, no gtest, no toolchain).
# Invoked from the repo root: make unit-test

UNIT_BUILD ?= $(ROOT_DIR)/build_unit
CC ?= gcc
UNIT_CFLAGS = -Wall -Wextra -Wno-unused-parameter -I$(ROOT_DIR)/src/common -I$(ROOT_DIR)/include -I$(ROOT_DIR)/test -DPLATFORM_LINUX -D_FILE_OFFSET_BITS=64

UNIT_STR_SRC = $(ROOT_DIR)/test/test_str.c $(ROOT_DIR)/src/common/utils/str.c
UNIT_FILE_SRC = $(ROOT_DIR)/test/test_file.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c \
	$(ROOT_DIR)/src/common/utils/log.c
UNIT_HASH_SRC = $(ROOT_DIR)/test/test_hash.c
UNIT_JSON_SRC = $(ROOT_DIR)/test/test_json.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c \
	$(ROOT_DIR)/src/common/utils/log.c \
	$(ROOT_DIR)/include/cjson/cJSON.c
UNIT_FLAGS_SRC = $(ROOT_DIR)/test/test_flags.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c \
	$(ROOT_DIR)/src/common/utils/log.c
UNIT_PROCESS_SRC = $(ROOT_DIR)/test/test_process.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c \
	$(ROOT_DIR)/src/common/utils/log.c
UNIT_STATE_SRC = $(ROOT_DIR)/test/test_state.c
UNIT_STR_SEC_SRC = $(ROOT_DIR)/test/test_str_security.c $(ROOT_DIR)/src/common/utils/str.c

.PHONY: unit-test unit-test-clean

unit-test:
	@$(makedir) $(UNIT_BUILD)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_str $(UNIT_STR_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_file $(UNIT_FILE_SRC)
	$(CC) $(UNIT_CFLAGS) -lm -o $(UNIT_BUILD)/test_hash $(UNIT_HASH_SRC)
	$(CC) $(UNIT_CFLAGS) -lm -o $(UNIT_BUILD)/test_json $(UNIT_JSON_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_flags $(UNIT_FLAGS_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_process $(UNIT_PROCESS_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_state $(UNIT_STATE_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_str_security $(UNIT_STR_SEC_SRC)
	@echo
	@$(UNIT_BUILD)/test_str
	@$(UNIT_BUILD)/test_file
	@$(UNIT_BUILD)/test_hash
	@$(UNIT_BUILD)/test_json
	@$(UNIT_BUILD)/test_flags
	@$(UNIT_BUILD)/test_process
	@$(UNIT_BUILD)/test_state
	@$(UNIT_BUILD)/test_str_security
	@echo
	@echo "unit-test: all host suites passed"

unit-test-clean:
	@rm -rf $(UNIT_BUILD)
