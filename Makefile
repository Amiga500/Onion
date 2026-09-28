###########################################################

TARGET=OnionPlus
VERSION=4.4.0-beta-20260823
RA_SUBVERSION=1.22.2-1

###########################################################

ifneq ($(VERSION_OVERRIDE),)
VERSION = $(VERSION_OVERRIDE)
endif
 
RELEASE_NAME := $(TARGET)-v$(VERSION)

ifdef OS
	current_dir := $(shell cd)
	ROOT_DIR := $(subst \,/,$(current_dir))
	makedir := mkdir
	createfile := echo.>
else
	ROOT_DIR := $(shell pwd)
	makedir := mkdir -p
	createfile := touch
endif

# Directories
SRC_DIR             := $(ROOT_DIR)/src
THIRD_PARTY_DIR     := $(ROOT_DIR)/third-party
BUILD_DIR           := $(ROOT_DIR)/build
BUILD_TEST_DIR      := $(ROOT_DIR)/build_test
TEST_SRC_DIR		:= $(ROOT_DIR)/test
BIN_DIR             := $(ROOT_DIR)/build/.tmp_update/bin
DIST_DIR            := $(ROOT_DIR)/dist
INSTALLER_DIR       := $(DIST_DIR)/miyoo/app/.tmp_update
RELEASE_DIR         := $(ROOT_DIR)/release
STATIC_BUILD        := $(ROOT_DIR)/static/build
STATIC_DIST         := $(ROOT_DIR)/static/dist
STATIC_CONFIGS      := $(ROOT_DIR)/static/configs
CACHE               := $(ROOT_DIR)/cache
STATIC_PACKAGES     := $(ROOT_DIR)/static/packages
PACKAGES_DIR        := $(ROOT_DIR)/build/App/PackageManager/data
PACKAGES_EMU_DEST   := $(PACKAGES_DIR)/Emu
PACKAGES_APP_DEST   := $(PACKAGES_DIR)/App
PACKAGES_RAPP_DEST  := $(PACKAGES_DIR)/RApp
TEMP_DIR            := $(ROOT_DIR)/cache/temp
INCLUDE_DIR         := $(ROOT_DIR)/include
ifeq (,$(GTEST_INCLUDE_DIR))
GTEST_INCLUDE_DIR = /usr/include/
endif

TOOLCHAIN := aemiii91/miyoomini-toolchain:latest

include ./src/common/commands.mk

###########################################################

.PHONY: all version core apps external release clean deepclean git-clean with-toolchain patch lib test unit-test jpg2png

all: dist

version: # used by workflow
	@echo $(VERSION)
print-version:
	@echo $(TARGET) v$(VERSION)
	@echo RetroArch sub-v$(RA_SUBVERSION)

# Setup copies static/, lib/ and the res/ + script/ folders of src/ into
# build/. It used to run only once (stamp file without prerequisites), so
# edits there were ignored until `make clean`. Now it re-runs whenever one
# of those inputs is newer than the stamp. (A find is used because many
# paths contain spaces, which make prerequisites cannot express.)
SETUP_INPUTS := $(STATIC_BUILD) $(STATIC_DIST) $(STATIC_CONFIGS) $(STATIC_PACKAGES) $(ROOT_DIR)/lib
ifeq ($(filter clean deepclean git-clean git-submodules version print-version pwd toolchain format,$(MAKECMDGOALS)),)
SETUP_STALE := $(shell [ -f $(CACHE)/.setup ] && { \
	find $(SETUP_INPUTS) -newer $(CACHE)/.setup -print -quit 2> /dev/null; \
	find $(SRC_DIR) \( -path '*/res/*' -o -path '*/script/*' \) -newer $(CACHE)/.setup -print -quit 2> /dev/null; } | head -n 1)
ifneq ($(SETUP_STALE),)
.PHONY: $(CACHE)/.setup
endif
endif

$(CACHE)/.setup:
	@$(ECHO) $(PRINT_RECIPE)
	@mkdir -p $(BUILD_DIR) $(DIST_DIR) $(RELEASE_DIR)
	@rsync -a --exclude='.gitkeep' $(STATIC_BUILD)/ $(BUILD_DIR)
	@rsync -a --exclude='.gitkeep' $(STATIC_DIST)/ $(DIST_DIR)
# Copy shared libraries
	@cp -R $(ROOT_DIR)/lib/. $(DIST_DIR)/miyoo/app/.tmp_update/lib
# Set version number
	@mkdir -p $(BUILD_DIR)/.tmp_update/onionVersion
	@echo -n "v$(VERSION)" > $(BUILD_DIR)/.tmp_update/onionVersion/version.txt
	@sed -i "s/{VERSION}/$(VERSION)/g" $(BUILD_DIR)/autorun.inf
# Copy all resources from src folders
	@find \
		$(SRC_DIR)/gameSwitcher \
		$(SRC_DIR)/chargingState \
		$(SRC_DIR)/bootScreen \
		$(SRC_DIR)/themeSwitcher \
		$(SRC_DIR)/tweaks \
		$(SRC_DIR)/randomGamePicker \
		$(SRC_DIR)/easter \
		-depth -type d -name res -exec cp -r {}/. $(BUILD_DIR)/.tmp_update/res/ \;
	@find \
		$(SRC_DIR)/packageManager \
		$(SRC_DIR)/themeSwitcher \
		-depth -type d -name script -exec cp -r {}/. $(BUILD_DIR)/.tmp_update/script/ \;
	@find $(SRC_DIR)/installUI -depth -type d -name res -exec cp -r {}/. $(INSTALLER_DIR)/res/ \;
# Download themes from theme repo
	@chmod a+x $(ROOT_DIR)/.github/get_themes.sh && $(ROOT_DIR)/.github/get_themes.sh
# Copy static configs
	@mkdir -p $(TEMP_DIR)/configs $(BUILD_DIR)/.tmp_update/config
	@rsync -a --exclude='.gitkeep' $(STATIC_CONFIGS)/ $(TEMP_DIR)/configs
# Copy static packages
	@mkdir -p $(PACKAGES_APP_DEST) $(PACKAGES_EMU_DEST) $(PACKAGES_RAPP_DEST)
	@rsync -a --exclude='.gitkeep' $(STATIC_PACKAGES)/App/ $(PACKAGES_APP_DEST)
	@rsync -a --exclude='.gitkeep' $(STATIC_PACKAGES)/Emu/ $(PACKAGES_EMU_DEST)
	@rsync -a --exclude='.gitkeep' $(STATIC_PACKAGES)/RApp/ $(PACKAGES_RAPP_DEST)
	@$(STATIC_PACKAGES)/common/apply.sh "$(PACKAGES_EMU_DEST)"
	@$(STATIC_PACKAGES)/common/apply.sh "$(PACKAGES_RAPP_DEST)"
	@$(STATIC_PACKAGES)/common/auto_advmenu_rc.sh "$(PACKAGES_EMU_DEST)" "$(TEMP_DIR)/configs/BIOS/.advance/advmenu.rc"
	@$(STATIC_PACKAGES)/common/auto_advmenu_rc.sh "$(PACKAGES_RAPP_DEST)" "$(TEMP_DIR)/configs/BIOS/.advance/advmenu.rc"
# Create full_resolution files
	@chmod a+x $(ROOT_DIR)/.github/create_fullres_files.sh && $(ROOT_DIR)/.github/create_fullres_files.sh
# Set flag: finished setup
	@touch $(CACHE)/.setup

build: core apps external
	@$(ECHO) $(PRINT_DONE)

# jpg2png is built like pngScale but not part of `core` until the Miyoo sysroot ships libjpeg.
jpg2png:
	@cd $(SRC_DIR)/jpg2png && BUILD_DIR=$(BIN_DIR) make

# Onion binaries. The two "prime" modules are built first, one after the
# other: between them they compile every object shared through ../common
# (str/log/file, cJSON, udp, retroarch_cmd) in the same order as before, so
# the remaining modules never compile the same shared .o concurrently and
# can be built in parallel with `make -j`. $(MAKE) (not `make`) passes the
# jobserver down to the sub-makes.
CORE_PRIME := bootScreen gameSwitcher
CORE_MODULES := chargingState \
	mainUiBatPerc \
	keymon \
	playActivity \
	themeSwitcher \
	tweaks \
	packageManager \
	sendkeys \
	setState \
	renameRom \
	infoPanel \
	prompt \
	batmon \
	easter \
	read_uuid \
	detectKey \
	axp \
	pressMenu2Kill \
	pngScale \
	libgamename \
	gameNameList \
	sendUDP \
	tree \
	pippi \
	cpuclock \
	fbmode
CORE_MODULE_TARGETS := $(addprefix core-mod-,$(CORE_MODULES))

.PHONY: core-prime core-modules $(CORE_MODULE_TARGETS)

core-prime: $(CACHE)/.setup
	@set -e; for m in $(CORE_PRIME); do (cd $(SRC_DIR)/$$m && BUILD_DIR=$(BIN_DIR) $(MAKE)); done

$(CORE_MODULE_TARGETS): core-mod-%: core-prime
	@cd $(SRC_DIR)/$* && BUILD_DIR=$(BIN_DIR) $(MAKE)

core-modules: $(CORE_MODULE_TARGETS)

core: core-modules
	@$(ECHO) $(PRINT_RECIPE)

# Build dependencies for installer
	@mkdir -p $(INSTALLER_DIR)/bin
	@cd $(SRC_DIR)/installUI && BUILD_DIR=$(INSTALLER_DIR)/bin/ VERSION=$(VERSION) make
	@cp $(BIN_DIR)/prompt $(INSTALLER_DIR)/bin/
	@cp $(BIN_DIR)/batmon $(INSTALLER_DIR)/bin/
	@cp $(BIN_DIR)/detectKey $(INSTALLER_DIR)/bin/
	@cp $(BIN_DIR)/infoPanel $(INSTALLER_DIR)/bin/
	@cp $(BIN_DIR)/gameNameList $(INSTALLER_DIR)/bin/
	@cp $(BIN_DIR)/playActivity $(INSTALLER_DIR)/bin/
	@cp $(BIN_DIR)/7z $(INSTALLER_DIR)/bin/
# Overrider miyoo libraries
	@cp $(BIN_DIR)/libgamename.so $(BUILD_DIR)/miyoo/lib/

apps: $(CACHE)/.setup
	@$(ECHO) $(PRINT_RECIPE)
	@cd $(SRC_DIR)/batteryMonitorUI && BUILD_DIR="$(PACKAGES_APP_DEST)/Battery Monitor/App/BatteryMonitorUI" make
	@find $(SRC_DIR)/batteryMonitorUI -depth -type d -name res -exec cp -r {}/. "$(PACKAGES_APP_DEST)/Battery Monitor/App/BatteryMonitorUI/res/" \;
	@cd $(SRC_DIR)/playActivityUI && BUILD_DIR="$(PACKAGES_APP_DEST)/Activity Tracker/App/PlayActivity" make
	@find $(SRC_DIR)/playActivityUI -depth -type d -name res -exec cp -r {}/. "$(PACKAGES_APP_DEST)/Activity Tracker/App/PlayActivity/res/" \;
	@find $(SRC_DIR)/packageManager -depth -type d -name res -exec cp -r {}/. $(BUILD_DIR)/App/PackageManager/res/ \;
	@cd $(SRC_DIR)/clock && BUILD_DIR="$(BIN_DIR)" make
	@cd $(SRC_DIR)/randomGamePicker && BUILD_DIR="$(BIN_DIR)" make
# Preinstalled apps
	@cp -a "$(PACKAGES_APP_DEST)/Activity Tracker/." $(BUILD_DIR)/
	@cp -a "$(PACKAGES_APP_DEST)/Quick Guide/." $(BUILD_DIR)/
	@cp -a "$(PACKAGES_APP_DEST)/RetroArch (Shortcut)/." $(BUILD_DIR)/
	@cp -a "$(PACKAGES_APP_DEST)/Tweaks/." $(BUILD_DIR)/
	@cp -a "$(PACKAGES_APP_DEST)/ThemeSwitcher/." $(BUILD_DIR)/

$(THIRD_PARTY_DIR)/RetroArch-patch/bin/retroarch_miyoo354:
	@$(ECHO) $(PRINT_RECIPE)
# RetroArch
	@$(ECHO) $(COLOR_BLUE)"\n-- Build RetroArch"$(COLOR_NORMAL)
	@cd $(THIRD_PARTY_DIR)/RetroArch-patch && make

external: $(CACHE)/.setup $(THIRD_PARTY_DIR)/RetroArch-patch/bin/retroarch_miyoo354
	@$(ECHO) $(PRINT_RECIPE)
# Add RetroArch
	@cp $(THIRD_PARTY_DIR)/RetroArch-patch/bin/* $(BUILD_DIR)/RetroArch/
	@echo $(RA_SUBVERSION) > $(BUILD_DIR)/RetroArch/onion_ra_version.txt
	@$(BUILD_DIR)/.tmp_update/script/build_ext_cache.sh $(BUILD_DIR)/RetroArch/.retroarch
# SearchFilter
	@$(ECHO) $(COLOR_BLUE)"\n-- Build SearchFilter"$(COLOR_NORMAL)
	@cd $(THIRD_PARTY_DIR)/SearchFilter && make build && cp -a build/. $(BUILD_DIR)
	@cp -a $(BUILD_DIR)/App/Search/. "$(PACKAGES_APP_DEST)/Search (Find your games)/App/Search"
	@mv -f $(BUILD_DIR)/App/Filter/* "$(PACKAGES_APP_DEST)/List shortcuts (Filter+Refresh)/App/Filter"
	@rmdir $(BUILD_DIR)/App/Filter
# Other
	@$(ECHO) $(COLOR_BLUE)"\n-- Build Terminal"$(COLOR_NORMAL)
	@cd $(THIRD_PARTY_DIR)/Terminal && make && cp ./st "$(BIN_DIR)"
	@$(ECHO) $(COLOR_BLUE)"\n-- Build DinguxCommander"$(COLOR_NORMAL)
	@cd $(THIRD_PARTY_DIR)/DinguxCommander && make && cp ./output/DinguxCommander "$(PACKAGES_APP_DEST)/File Explorer (DinguxCommander)/App/Commander_Italic"

dist: build
	@$(ECHO) $(PRINT_RECIPE)
# Package configs
	@cp -R $(TEMP_DIR)/configs/Saves/CurrentProfile/ $(TEMP_DIR)/configs/Saves/GuestProfile
	@echo -n "Packaging configs..."
	@cd $(TEMP_DIR)/configs && 7z a -mtm=off $(BUILD_DIR)/.tmp_update/config/configs.pak . -bsp1 -bso0
	@echo " DONE"
	@rm -rf $(TEMP_DIR)/configs
	@rmdir $(TEMP_DIR)
# Package RetroArch separately
# Recompressing RetroArch is the slowest packaging step and its content
# rarely changes: reuse the cached archive while a content hash (paths,
# modes, symlink targets, file data; mtimes are not archived) is identical.
	@echo -n "Packaging RetroArch..."
	@mkdir -p $(CACHE) $(DIST_DIR)/RetroArch
	@ra_hash=$$(cd $(BUILD_DIR) && { \
		find ./RetroArch \( -type f -o -type l \) -printf '%p %m %l\n' | LC_ALL=C sort; \
		find ./RetroArch -type f -print0 | LC_ALL=C sort -z | xargs -0 -r sha1sum; \
	} | sha1sum | cut -d' ' -f1); \
	if [ -f $(CACHE)/retroarch.pak ] && [ "$$(cat $(CACHE)/retroarch.pak.sha1 2> /dev/null)" = "$$ra_hash" ]; then \
		echo -n " unchanged, cached archive reused."; \
	else \
		rm -f $(CACHE)/retroarch.pak $(CACHE)/retroarch.pak.sha1; \
		(cd $(BUILD_DIR) && 7z a -mtm=off $(CACHE)/retroarch.pak ./RetroArch -bsp1 -bso0) || exit 1; \
		echo "$$ra_hash" > $(CACHE)/retroarch.pak.sha1; \
	fi
	@echo " DONE"
	@cp $(CACHE)/retroarch.pak $(DIST_DIR)/RetroArch/
	@echo $(RA_SUBVERSION) > $(DIST_DIR)/RetroArch/ra_package_version.txt
# Package Onion core
	@echo -n "Packaging Onion..."
	@cd $(BUILD_DIR) && 7z a -mtm=off $(DIST_DIR)/miyoo/app/.tmp_update/onion.pak . -x!RetroArch -bsp1 -bso0
	@echo " DONE"
	@$(ECHO) $(PRINT_DONE)

release: dist
	@$(ECHO) $(PRINT_RECIPE)
	@rm -f $(RELEASE_DIR)/$(RELEASE_NAME).zip
	@cd $(DIST_DIR) && 7z a -mtc=off $(RELEASE_DIR)/$(RELEASE_NAME).zip . -bsp1 -bso0
	@$(ECHO) "Release zip: $(RELEASE_DIR)/$(RELEASE_NAME).zip"
	@$(ECHO) $(PRINT_DONE)

clean:
	@$(ECHO) $(PRINT_RECIPE)
	@rm -rf $(BUILD_DIR) $(BUILD_TEST_DIR) $(ROOT_DIR)/dist $(TEMP_DIR)/configs
	@rm -f $(CACHE)/.setup
	@find include src -type f -name '*.o' -exec rm -f {} +

deepclean: clean
	@rm -rf $(CACHE)
	@cd $(THIRD_PARTY_DIR)/RetroArch-patch && make clean
	@cd $(THIRD_PARTY_DIR)/SearchFilter && make clean
	@cd $(THIRD_PARTY_DIR)/Terminal && make clean
	@cd $(THIRD_PARTY_DIR)/DinguxCommander && make clean

dev: clean
	@$(MAKE_DEV)

asan: clean
	@$(MAKE_ASAN)
	
git-clean:
	@git clean -xfd -e .vscode

git-submodules:
	@git submodule update --init --recursive

pwd:
	@echo $(ROOT_DIR)

$(CACHE)/.docker:
	docker pull $(TOOLCHAIN)
	$(makedir) cache
	$(createfile) $(CACHE)/.docker

toolchain: $(CACHE)/.docker
	docker run -it --rm -v "$(ROOT_DIR)":/root/workspace $(TOOLCHAIN) /bin/bash

with-toolchain: $(CACHE)/.docker
	docker run --rm -v "$(ROOT_DIR)":/root/workspace $(TOOLCHAIN) /bin/bash -c "source /root/.bashrc; make $(CMD)"

patch:
	@chmod a+x $(ROOT_DIR)/.github/create_patch.sh && $(ROOT_DIR)/.github/create_patch.sh

external-libs:
	@cd $(ROOT_DIR)/include/SDL && make clean && make

unit-test:
	@cd $(ROOT_DIR)/test && $(MAKE) -f Makefile.unit all

test: external-libs
	@mkdir -p $(BUILD_TEST_DIR)/infoPanel_test_data && cd $(TEST_SRC_DIR) && BUILD_DIR=$(BUILD_TEST_DIR)/ make dev
	@cp -R $(TEST_SRC_DIR)/infoPanel_test_data $(BUILD_TEST_DIR)/
	cd $(BUILD_TEST_DIR) && LD_LIBRARY_PATH=$(ROOT_DIR)/lib/ ./test

static-analysis: external-libs
	@cd $(ROOT_DIR) && cppcheck -I $(INCLUDE_DIR) --enable=all $(SRC_DIR)

format:
	@find ./src -regex '.*\.\(c\|h\|cpp\|hpp\)' -exec clang-format -style=file -i {} \;
