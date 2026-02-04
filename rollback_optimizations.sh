#!/bin/bash
# Rollback performance optimizations

echo "Rolling back performance optimizations..."

if [ -f "src/gameSwitcher/gameSwitcher.c.backup" ]; then
    cp src/gameSwitcher/gameSwitcher.c.backup src/gameSwitcher/gameSwitcher.c
    echo "✓ gameSwitcher.c restored"
fi

if [ -f "src/common/config.mk.backup" ]; then
    cp src/common/config.mk.backup src/common/config.mk
    echo "✓ config.mk restored"
fi

# Clean build
make -C src/gameSwitcher clean

echo "✓ Rollback complete"
