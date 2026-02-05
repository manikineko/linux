#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Build script for Kernel Shell modules
#
# This script helps build the kernel shell modules with proper
# configuration for Lua and QuickJS integration.

set -e

KERNEL_DIR=$(pwd)/../../../..
SCRIPT_DIR=$(dirname "$0")
cd "$SCRIPT_DIR"

echo "Building Kernel Shell modules..."
echo "Kernel directory: $KERNEL_DIR"
echo "Script directory: $SCRIPT_DIR"

# Check if we're in the kernel source directory
if [ ! -f "$KERNEL_DIR/Makefile" ]; then
    echo "Error: Not in kernel source directory"
    echo "Expected kernel Makefile at: $KERNEL_DIR/Makefile"
    exit 1
fi

# Check for required source files
if [ ! -d "lua-5.5.0" ]; then
    echo "Error: Lua source code not found"
    echo "Please ensure lua-5.5.0.tar.gz is extracted"
    exit 1
fi

if [ ! -d "quickjs-2024-01-13" ]; then
    echo "Error: QuickJS source code not found"
    echo "Please ensure quickjs-2024-01-13.tar.xz is extracted"
    exit 1
fi

# Build configuration
echo "Configuring kernel build..."

# Prepare kernel configuration
cd "$KERNEL_DIR"

# Enable kernel shell options if not already enabled
if ! grep -q "CONFIG_KERNEL_SHELL=y" .config 2>/dev/null; then
    echo "Enabling kernel shell configuration..."
    echo "CONFIG_KERNEL_SHELL=y" >> .config
    echo "CONFIG_KERNEL_SHELL_LOGO=y" >> .config
    echo "CONFIG_KERNEL_SHELL_SCRIPTING=y" >> .config
    echo "CONFIG_KERNEL_SHELL_LUA=y" >> .config
    echo "CONFIG_KERNEL_SHELL_NODEJS=y" >> .config
    echo "CONFIG_KERNEL_SHELL_LUA_REAL_MODULE=m" >> .config
    echo "CONFIG_KERNEL_SHELL_NODEJS_REAL_MODULE=m" >> .config
    echo "CONFIG_MODULES=y" >> .config
    echo "CONFIG_MODULE_UNLOAD=y" >> .config
fi

# Build the modules
echo "Building kernel shell modules..."

# Build demo modules
echo "Building demo modules..."
make M=drivers/kernel_shell/kernel_shell_lua.ko
make M=drivers/kernel_shell/kernel_shell_nodejs.ko

# Build real modules (this may take longer)
echo "Building real Lua module..."
make M=drivers/kernel_shell/kernel_shell_lua_real.ko

echo "Building real QuickJS module..."
make M=drivers/kernel_shell/kernel_shell_nodejs_real.ko

# Check build results
echo "Checking build results..."
cd "$SCRIPT_DIR"

DEMO_MODULES=(
    "kernel_shell_lua.ko"
    "kernel_shell_nodejs.ko"
)

REAL_MODULES=(
    "kernel_shell_lua_real.ko"
    "kernel_shell_nodejs_real.ko"
)

echo "Demo modules:"
for module in "${DEMO_MODULES[@]}"; do
    if [ -f "$module" ]; then
        size=$(stat -c%s "$module" 2>/dev/null || echo "unknown")
        echo "  ✓ $module (${size} bytes)"
    else
        echo "  ✗ $module (not built)"
    fi
done

echo "Real modules:"
for module in "${REAL_MODULES[@]}"; do
    if [ -f "$module" ]; then
        size=$(stat -c%s "$module" 2>/dev/null || echo "unknown")
        echo "  ✓ $module (${size} bytes)"
    else
        echo "  ✗ $module (not built)"
    fi
done

# Installation instructions
echo ""
echo "Build completed!"
echo ""
echo "To install the modules:"
echo "  sudo make -C $KERNEL_DIR M=$SCRIPT_DIR modules_install"
echo "  sudo depmod -a"
echo ""
echo "To load the modules:"
echo "  sudo modprobe kernel_shell_lua      # Demo Lua"
echo "  sudo modprobe kernel_shell_nodejs   # Demo Node.js"
echo "  sudo modprobe kernel_shell_lua_real # Real Lua"
echo "  sudo modprobe kernel_shell_nodejs_real # Real QuickJS"
echo ""
echo "To test the kernel shell:"
echo "  echo 'c' | sudo tee /proc/sys/kernel/panic  # Test panic (CAUTION!)"
echo ""
echo "Note: Real modules provide full language support but are larger."
echo "      Demo modules are smaller but have limited functionality."
