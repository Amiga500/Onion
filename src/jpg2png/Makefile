INCLUDE_UTILS=0
include ../common/config.mk

TARGET = jpg2png
LDFLAGS := $(LDFLAGS) -lmi_sys -lmi_gfx -lpng -Wl,-Bstatic -ljpeg -Wl,-Bdynamic

include ../common/commands.mk
include ../common/recipes.mk
