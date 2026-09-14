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
UNIT_LOG_SRC = $(ROOT_DIR)/test/test_log.c \
	$(ROOT_DIR)/src/common/utils/log.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c
UNIT_CONFIG_SRC = $(ROOT_DIR)/test/test_config.c \
	$(ROOT_DIR)/src/common/utils/file.c \
	$(ROOT_DIR)/src/common/utils/str.c \
	$(ROOT_DIR)/src/common/utils/log.c
UNIT_CONFIG_CFLAGS = $(UNIT_CFLAGS) -DCONFIG_PATH=\"/tmp/onion_unit_config/\"

.PHONY: unit-test unit-test-asan unit-test-clean check-no-system

check-no-system:
	@hits=$$(grep -RInE '(^|[^[:alnum:]_])system\(' $(ROOT_DIR)/src --include='*.c' --include='*.h' || true); \
	if [ -n "$$hits" ]; then \
		echo "$$hits"; \
		echo "check-no-system: leftover system() in src/"; \
		exit 1; \
	fi; \
	echo "check-no-system: ok"

unit-test: check-no-system
	@$(makedir) $(UNIT_BUILD)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_str $(UNIT_STR_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_file $(UNIT_FILE_SRC)
	$(CC) $(UNIT_CFLAGS) -lm -o $(UNIT_BUILD)/test_hash $(UNIT_HASH_SRC)
	$(CC) $(UNIT_CFLAGS) -lm -o $(UNIT_BUILD)/test_json $(UNIT_JSON_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_flags $(UNIT_FLAGS_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_process $(UNIT_PROCESS_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_state $(UNIT_STATE_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_str_security $(UNIT_STR_SEC_SRC)
	$(CC) $(UNIT_CFLAGS) -o $(UNIT_BUILD)/test_log $(UNIT_LOG_SRC)
	$(CC) $(UNIT_CONFIG_CFLAGS) -o $(UNIT_BUILD)/test_config $(UNIT_CONFIG_SRC)
	@echo
	@$(UNIT_BUILD)/test_str
	@$(UNIT_BUILD)/test_file
	@$(UNIT_BUILD)/test_hash
	@$(UNIT_BUILD)/test_json
	@$(UNIT_BUILD)/test_flags
	@$(UNIT_BUILD)/test_process
	@$(UNIT_BUILD)/test_state
	@$(UNIT_BUILD)/test_str_security
	@$(UNIT_BUILD)/test_log
	@$(UNIT_BUILD)/test_config
	@echo
	@echo "unit-test: all host suites passed"

unit-test-asan:
	$(MAKE) unit-test UNIT_BUILD=$(ROOT_DIR)/build_unit_asan \
		UNIT_CFLAGS="$(UNIT_CFLAGS) -fsanitize=address -fno-omit-frame-pointer"

unit-test-clean:
	@rm -rf $(UNIT_BUILD) $(ROOT_DIR)/build_unit_asan
