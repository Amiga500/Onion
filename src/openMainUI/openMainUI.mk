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
		echo "third-party/open-mainui is empty: the Open MainUI subtree is missing"; \
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

# Tweaks > Appearance > Game list writes the files Open MainUI reads in
# src/core/config.c. Stop when that file (or docs/TIMING.md, which lists the
# exact scroll speeds) changed since the menu was last checked. After review,
# refresh the fingerprint with: make open-mainui-config-accept
OPEN_MAINUI_CONFIG_SHA := $(ROOT_DIR)/src/openMainUI/config.sha256
.PHONY: open-mainui-config-check open-mainui-config-accept
open-mainui-config-check:
	@cd $(OPEN_MAINUI_DIR) && sha256sum -c --status $(OPEN_MAINUI_CONFIG_SHA) || { \
		echo "Open MainUI src/core/config.c or docs/TIMING.md changed since the"; \
		echo "Tweaks > Game list menu was checked. Review the diff, adapt the menu"; \
		echo "if needed, then run: make open-mainui-config-accept"; \
		exit 1; }
open-mainui-config-accept:
	@cd $(OPEN_MAINUI_DIR) && sha256sum src/core/config.c docs/TIMING.md > $(OPEN_MAINUI_CONFIG_SHA)
	@echo "Open MainUI config fingerprint updated."
open-mainui: open-mainui-config-check
