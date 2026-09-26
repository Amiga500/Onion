# Open MainUI test integration (branch test/open-mainui only).
# Included at the end of the top-level Makefile. OPEN_MAINUI=0 builds stock.
#
# Runs inside the same toolchain container as the rest of Onion, e.g.
#   make with-toolchain CMD=release
# Open MainUI's own `make device` checks CROSS_COMPILE and that ONION_ROOT
# provides lib/libsqlite3.so, lib/libshmvar.so and include/sqlite3/sqlite3.h.

OPEN_MAINUI ?= 1
OPEN_MAINUI_DIR := $(THIRD_PARTY_DIR)/open-mainui
OPEN_MAINUI_BIN := $(OPEN_MAINUI_DIR)/build/onion/MainUI

.PHONY: open-mainui open-mainui-clean

# After `build`, so every MainUI-* variant is already in BIN_DIR.
# The sub-make gets a clean environment: Onion's flags and command-line
# variables (VERSION, CFLAGS, ...) must not leak into Open MainUI's build.
open-mainui: build
	@$(ECHO) $(PRINT_RECIPE)
	@test -f $(OPEN_MAINUI_DIR)/Makefile || { \
		echo "third-party/open-mainui is empty: run 'git submodule update --init third-party/open-mainui'"; \
		exit 1; }
	@cd $(OPEN_MAINUI_DIR) && env -u MAKEFLAGS -u MFLAGS -u MAKELEVEL -u MAKEOVERRIDES \
		-u CFLAGS -u CPPFLAGS -u LDFLAGS -u LDLIBS -u CC -u AR \
		make device ONION_ROOT=$(ROOT_DIR)
	@sh $(SRC_DIR)/openMainUI/install.sh "$(BIN_DIR)" "$(BUILD_DIR)" \
		"$(OPEN_MAINUI_BIN)" "$(SRC_DIR)/openMainUI/MainUI-wrapper.sh.in"

open-mainui-clean:
	@cd $(OPEN_MAINUI_DIR) && make clean

ifeq ($(OPEN_MAINUI),1)
dist: open-mainui
endif
