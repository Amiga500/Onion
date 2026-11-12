# pdflight links to the poppler-glib and cairo that ship for Green in
# static/build/.tmp_update/lib and lib/parasyte (no headers needed: libs.h).
ifeq (,$(BUILD_DIR))
BUILD_DIR=$(shell pwd -P)
endif

CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
LIBS_DIR := ../../static/build/.tmp_update/lib
CFLAGS := -O2 -Wall -Wno-unused-function
LDFLAGS := -L$(LIBS_DIR) -L$(LIBS_DIR)/parasyte -Wl,-rpath-link,$(LIBS_DIR):$(LIBS_DIR)/parasyte:../../static/build/miyoo/lib \
	-l:libpoppler-glib.so.8 -l:libgobject-2.0.so.0 -l:libglib-2.0.so.0 -l:libcairo.so.2 -lm

all: $(BUILD_DIR)/green/pdflight

$(BUILD_DIR)/green/pdflight: pdflight.c libs.h ../pngresize/stb_image_write.h
	@mkdir -p $(BUILD_DIR)/green
	$(CC) $(CFLAGS) -o $@ pdflight.c $(LDFLAGS)
	$(STRIP) $@

clean:
	rm -f $(BUILD_DIR)/green/pdflight pdflight

.PHONY: all clean
