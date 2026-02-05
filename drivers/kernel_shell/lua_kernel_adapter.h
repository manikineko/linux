/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Lua Kernel Adapter
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This adapter provides kernel-space compatibility for Lua interpreter.
 */

#ifndef _LUA_KERNEL_ADAPTER_H
#define _LUA_KERNEL_ADAPTER_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

// Kernel space memory allocation for Lua
#ifdef KERNEL_LUA_ADAPTER

// Replace standard library functions with kernel equivalents
#define malloc(size) kmalloc(size, GFP_KERNEL)
#define free(ptr) kfree(ptr)
#define realloc(ptr, size) krealloc(ptr, size, GFP_KERNEL)
#define strdup(s) kstrdup(s, GFP_KERNEL)

// Replace I/O functions
#define printf(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define fprintf(fp, fmt, ...) pr_info(fmt, ##__VA_ARGS__)

// Replace assert
#define assert(cond) BUG_ON(!(cond))

// Kernel-specific time functions
#define time_t unsigned long
#define time(t) (jiffies / HZ)

// Kernel-specific file operations (disabled for safety)
#define fopen(path, mode) NULL
#define fclose(fp) do {} while(0)
#define fread(ptr, size, n, fp) 0
#define fwrite(ptr, size, n, fp) 0
#define fseek(fp, offset, whence) -1
#define ftell(fp) -1

// Disable dynamic loading
#define dlopen(filename, flag) NULL
#define dlsym(handle, symbol) NULL
#define dlclose(handle) do {} while(0)

// Kernel-specific string functions (if needed)
#define strerror(errnum) "kernel error"

#endif // KERNEL_LUA_ADAPTER

#endif // _LUA_KERNEL_ADAPTER_H
