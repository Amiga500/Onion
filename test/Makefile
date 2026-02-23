# Check if gtest and SDL are available (once at parse time)
GTEST_AVAILABLE := $(shell $(CROSS_COMPILE)g++ -include gtest/gtest.h -x c++ -c /dev/null -o /dev/null 2>/dev/null && echo yes)
SDL_AVAILABLE := $(shell $(CROSS_COMPILE)gcc -include SDL/SDL.h -x c -c /dev/null -o /dev/null 2>/dev/null && echo yes)

.PHONY: all check-deps clean

# Default target when running 'make' without arguments
all: check-deps

# Check if gtest and SDL are available, then build or skip
check-deps:
	@if [ "$(GTEST_AVAILABLE)" = "yes" ] && [ "$(SDL_AVAILABLE)" = "yes" ]; then \
		echo "-- GTest and SDL found, building test_infoPanel"; \
		$(MAKE) -f Makefile.gtest; \
	elif [ "$(GTEST_AVAILABLE)" != "yes" ]; then \
		echo "-- GTest not found, skipping test_infoPanel (install libgtest-dev to enable)"; \
	else \
		echo "-- SDL headers not found, skipping test_infoPanel (install libsdl1.2-dev to enable)"; \
	fi

# Allow clean to work without gtest
clean:
	@$(MAKE) -f Makefile.gtest clean 2>/dev/null || true

# Pass build-related targets to Makefile.gtest if gtest and SDL are available
dev asan build:
	@if [ "$(GTEST_AVAILABLE)" = "yes" ] && [ "$(SDL_AVAILABLE)" = "yes" ]; then \
		$(MAKE) -f Makefile.gtest $@; \
	elif [ "$(GTEST_AVAILABLE)" != "yes" ]; then \
		echo "-- GTest not found, cannot build test_infoPanel (install libgtest-dev to enable)"; \
		exit 1; \
	else \
		echo "-- SDL headers not found, cannot build test_infoPanel (install libsdl1.2-dev to enable)"; \
		exit 1; \
	fi