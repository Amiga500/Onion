#!/bin/bash
# Build Onion with performance optimizations enabled

echo "Building Onion with performance optimizations..."
echo ""

# Clean previous build
make -C src/gameSwitcher clean

# Build with optimizations
make -C src/gameSwitcher PLATFORM=miyoomini OPTIMIZE_SAVE=1

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful with optimizations enabled"
    echo ""
    echo "Binary location: src/gameSwitcher/gameSwitcher"
    ls -lh src/gameSwitcher/gameSwitcher
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi
