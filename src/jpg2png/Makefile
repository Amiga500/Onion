include ../common/config.mk

# Same pattern as pngScale. Needs ARM libjpeg in the sysroot (-ljpeg).
# The stock aemiii91/miyoomini-toolchain image does not ship it; the
# jpg2png binary must be rebuilt on a sysroot that has libjpeg (or via
# CI/pre-release once that is available).
TARGET = jpg2png
LDFLAGS := $(LDFLAGS) -lmi_sys -lmi_gfx -lpng -Wl,-Bstatic -ljpeg -Wl,-Bdynamic

include ../common/commands.mk
include ../common/recipes.mk
