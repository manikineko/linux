# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for Kernel Shell Drivers
#

# Demo modules (simplified implementations)
obj-$(CONFIG_KERNEL_SHELL_LUA_MODULE) += kernel_shell_lua.o
obj-$(CONFIG_KERNEL_SHELL_NODEJS_MODULE) += kernel_shell_nodejs.o

# Real modules with actual language integration - disabled for now
# obj-$(CONFIG_KERNEL_SHELL_LUA_REAL_MODULE) += kernel_shell_lua_real.o
# obj-$(CONFIG_KERNEL_SHELL_NODEJS_REAL_MODULE) += kernel_shell_nodejs_real.o

# Example scriptable module
obj-$(CONFIG_EXAMPLE_SCRIPTABLE_MODULE) += example_scriptable_module.o

# Test module for kernel shell functionality
obj-$(CONFIG_KERNEL_SHELL_TEST_MODULE) += kernel_shell_test.o

# Add source directories for language engines (commented out for now)
# ccflags-y += -I$(srctree)/drivers/kernel_shell/lua-5.5.0/src
# ccflags-y += -I$(srctree)/drivers/kernel_shell/quickjs-2024-01-13

# Lua source files for real integration (commented out for now)
# kernel_shell_lua_real-objs := \
#	lua-5.5.0/src/lapi.o \
#	lua-5.5.0/src/lcode.o \
#	lua-5.5.0/src/lctype.o \
#	lua-5.5.0/src/ldebug.o \
#	lua-5.5.0/src/ldo.o \
#	lua-5.5.0/src/ldump.o \
#	lua-5.5.0/src/lfunc.o \
#	lua-5.5.0/src/lgc.o \
#	lua-5.5.0/src/llex.o \
#	lua-5.5.0/src/lmem.o \
#	lua-5.5.0/src/lobject.o \
#	lua-5.5.0/src/lopcodes.o \
#	lua-5.5.0/src/lparser.o \
#	lua-5.5.0/src/lstate.o \
#	lua-5.5.0/src/lstring.o \
#	lua-5.5.0/src/ltable.o \
#	lua-5.5.0/src/ltm.o \
#	lua-5.5.0/src/lundump.o \
#	lua-5.5.0/src/lvm.o \
#	lua-5.5.0/src/lzio.o \
#	lua-5.5.0/src/lauxlib.o \
#	lua-5.5.0/src/lbaselib.o \
#	lua-5.5.0/src/lcorolib.o \
#	lua-5.5.0/src/ldblib.o \
#	lua-5.5.0/src/liolib.o \
#	lua-5.5.0/src/lmathlib.o \
#	lua-5.5.0/src/loslib.o \
#	lua-5.5.0/src/lstrlib.o \
#	lua-5.5.0/src/ltablib.o \
#	lua-5.5.0/src/lutf8lib.o \
#	lua-5.5.0/src/linit.o \
#	lua_kernel_patch.o

# QuickJS source files for real integration (commented out for now)
# kernel_shell_nodejs_real-objs := \
#	quickjs-2024-01-13/quickjs.o \
#	quickjs-2024-01-13/quickjs-libc.o \
#	quickjs-2024-01-13/cutils.o \
#	quickjs-2024-01-13/libbf.o \
#	quickjs-2024-01-13/libregexp.o \
#	quickjs-2024-01-13/libunicode.o \
#	quickjs_kernel_patch.o

# Compiler flags for kernel adaptation (commented out for now)
# ccflags-y += -DLUA_USE_LINUX
# ccflags-y += -DCONFIG_KERNEL_SHELL
# ccflags-y += -DKERNEL_LUA_ADAPTER

# Disable features not suitable for kernel space (commented out for now)
# ccflags-y += -DLUA_NO_DLOPEN
# ccflags-y += -DLUA_USE_POSIX
# ccflags-y += -DLUA_USE_STRTODHEAP

# QuickJS compiler flags (commented out for now)
# ccflags-y += -DCONFIG_VERSION=\"$(shell cat $(srctree)/drivers/kernel_shell/quickjs-2024-01-13/VERSION)\"
# ccflags-y += -D_GNU_SOURCE
# ccflags-y += -Wno-pointer-sign
