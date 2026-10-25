# SDL video warmup uses no Onion common utilities.
INCLUDE_UTILS=0
include ../common/config.mk

TARGET = warmup
# clock_gettime may live in librt with older Linux cross-toolchains.
LDFLAGS := $(LDFLAGS) -lSDL -lrt

include ../common/commands.mk
include ../common/recipes.mk
