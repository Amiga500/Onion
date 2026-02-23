# Check if gtest is available (once at parse time)
GTEST_AVAILABLE := $(shell echo '#include <gtest/gtest.h>' | $(CROSS_COMPILE)g++ -x c++ -c - -o /dev/null 2>/dev/null && echo yes)

.PHONY: all check-gtest clean

# Default target when running 'make' without arguments
all: check-gtest

# Check if gtest is available, then build or skip
check-gtest:
	@if [ "$(GTEST_AVAILABLE)" = "yes" ]; then \
		echo "-- GTest found, building test_infoPanel"; \
		$(MAKE) -f Makefile.gtest; \
	else \
		echo "-- GTest not found, skipping test_infoPanel (install libgtest-dev to enable)"; \
	fi

# Allow clean to work without gtest
clean:
	@$(MAKE) -f Makefile.gtest clean 2>/dev/null || true

# Pass build-related targets to Makefile.gtest if gtest is available
dev asan build:
	@if [ "$(GTEST_AVAILABLE)" = "yes" ]; then \
		$(MAKE) -f Makefile.gtest $@; \
	else \
		echo "-- GTest not found, cannot build test_infoPanel (install libgtest-dev to enable)"; \
		exit 1; \
	fi