/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Lua Kernel Space Patches
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This file provides kernel space implementations for Lua functions
 * that are not compatible with the kernel environment.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include "lua_kernel_adapter.h"

// Kernel space implementations for Lua standard library functions

// Replace lua_writestring with kernel print
void lua_writestring(const char *s, size_t len)
{
    if (s && len > 0) {
        pr_info("%.*s", (int)len, s);
    }
}

void lua_writeline(void)
{
    pr_info("\n");
}

void lua_writestringerror(const char *fmt, ...)
{
    // In kernel space, we can't use varargs easily, so just print a generic error
    pr_err("Lua error occurred\n");
}

// Replace lua_assert
void lua_assert(long cond)
{
    if (!cond) {
        BUG();
    }
}

// Kernel-specific file operations (disabled for safety)
FILE *lua_fopen(const char *filename, const char *mode)
{
    // File operations disabled in kernel space for safety
    return NULL;
}

int lua_fclose(FILE *fp)
{
    return 0;
}

size_t lua_fread(void *ptr, size_t size, size_t n, FILE *fp)
{
    return 0;
}

size_t lua_fwrite(const void *ptr, size_t size, size_t n, FILE *fp)
{
    return 0;
}

int lua_fseek(FILE *fp, long offset, int whence)
{
    return -1;
}

long lua_ftell(FILE *fp)
{
    return -1;
}

// Kernel-specific system functions
int lua_system(const char *cmd)
{
    // System commands disabled in kernel space for safety
    return -1;
}

// Kernel-specific exit function
void lua_exit(int status)
{
    pr_err("Lua: exit called with status %d\n", status);
    // Don't actually exit the kernel
}

// Kernel-specific getenv function
char *lua_getenv(const char *name)
{
    // Environment variables not available in kernel space
    return NULL;
}

// Kernel-specific time functions
double lua_time(void)
{
    return (double)jiffies / HZ;
}

// Kernel-specific random number generator
int lua_rand(void)
{
    return get_random_u32();
}

void lua_srand(unsigned int seed)
{
    // Seed is handled by kernel's random number generator
}

// Memory allocation tracking for debugging
#ifdef DEBUG_LUA_MEMORY
static atomic_t lua_alloc_count = ATOMIC_INIT(0);
static atomic_t lua_alloc_bytes = ATOMIC_INIT(0);

void *lua_malloc_kernel(size_t size)
{
    void *ptr = kmalloc(size, GFP_KERNEL);
    if (ptr) {
        atomic_inc(&lua_alloc_count);
        atomic_add(size, &lua_alloc_bytes);
        pr_debug("Lua malloc: %p (%zu bytes, total: %d allocations, %d bytes)\n",
                ptr, size, atomic_read(&lua_alloc_count), atomic_read(&lua_alloc_bytes));
    }
    return ptr;
}

void lua_free_kernel(void *ptr)
{
    if (ptr) {
        atomic_dec(&lua_alloc_count);
        pr_debug("Lua free: %p (total: %d allocations)\n", ptr, atomic_read(&lua_alloc_count));
        kfree(ptr);
    }
}

void *lua_realloc_kernel(void *ptr, size_t size)
{
    void *new_ptr = krealloc(ptr, size, GFP_KERNEL);
    if (new_ptr) {
        pr_debug("Lua realloc: %p -> %p (%zu bytes)\n", ptr, new_ptr, size);
    }
    return new_ptr;
}

#else
// Non-debug versions
void *lua_malloc_kernel(size_t size)
{
    return kmalloc(size, GFP_KERNEL);
}

void lua_free_kernel(void *ptr)
{
    kfree(ptr);
}

void *lua_realloc_kernel(void *ptr, size_t size)
{
    return krealloc(ptr, size, GFP_KERNEL);
}
#endif

// String operations
char *lua_strdup_kernel(const char *s)
{
    return kstrdup(s, GFP_KERNEL);
}

// Kernel-specific math functions
double lua_sqrt(double x)
{
    return sqrt(x);
}

double lua_pow(double x, double y)
{
    return pow(x, y);
}

double lua_log(double x)
{
    return log(x);
}

double lua_log10(double x)
{
    return log10(x);
}

double lua_exp(double x)
{
    return exp(x);
}

double lua_sin(double x)
{
    return sin(x);
}

double lua_cos(double x)
{
    return cos(x);
}

double lua_tan(double x)
{
    return tan(x);
}

double lua_asin(double x)
{
    return asin(x);
}

double lua_acos(double x)
{
    return acos(x);
}

double lua_atan(double x)
{
    return atan(x);
}

double lua_atan2(double y, double x)
{
    return atan2(y, x);
}

double lua_fabs(double x)
{
    return fabs(x);
}

double lua_floor(double x)
{
    return floor(x);
}

double lua_ceil(double x)
{
    return ceil(x);
}
