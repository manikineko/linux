/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * QuickJS Kernel Adapter
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This adapter provides kernel-space compatibility for QuickJS interpreter.
 */

#ifndef _QUICKJS_KERNEL_ADAPTER_H
#define _QUICKJS_KERNEL_ADAPTER_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/limits.h>

// Kernel space memory allocation for QuickJS
#ifdef CONFIG_KERNEL_SHELL

// Replace standard library functions with kernel equivalents
#define malloc(size) kmalloc(size, GFP_KERNEL)
#define free(ptr) kfree(ptr)
#define realloc(ptr, size) krealloc(ptr, size, GFP_KERNEL)
#define calloc(n, size) kzalloc((n) * (size), GFP_KERNEL)

// Replace I/O functions
#define printf(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define fprintf(fp, fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define sprintf(buf, fmt, ...) snprintf(buf, PAGE_SIZE, fmt, ##__VA_ARGS__)
#define snprintf(buf, size, fmt, ...) scnprintf(buf, size, fmt, ##__VA_ARGS__)

// Replace assert
#define assert(cond) BUG_ON(!(cond))

// Kernel-specific time functions
#define time_t unsigned long
#define time(t) (jiffies / HZ)
#define clock() jiffies

// Kernel-specific file operations (disabled for safety)
#define fopen(path, mode) NULL
#define fclose(fp) do {} while(0)
#define fread(ptr, size, n, fp) 0
#define fwrite(ptr, size, n, fp) 0
#define fseek(fp, offset, whence) -1
#define ftell(fp) -1
#define fileno(fp) -1

// Kernel-specific string functions
#define strerror(errnum) "kernel error"
#define getenv(name) NULL

// Kernel-specific math functions
#define isnan(x) kernel_isnan(x)
#define isinf(x) kernel_isinf(x)
#define isfinite(x) kernel_isfinite(x)

static inline int kernel_isnan(double x)
{
    return x != x; // NaN comparison
}

static inline int kernel_isinf(double x)
{
    return x == (1.0/0.0) || x == (-1.0/0.0);
}

static inline int kernel_isfinite(double x)
{
    return !kernel_isnan(x) && !kernel_isinf(x);
}

// Kernel-specific random number generator
#define rand() get_random_u32()
#define srand(seed) do {} while(0)

// Kernel-specific exit
#define exit(code) do { pr_err("QuickJS: exit called with code %d\n", code); } while(0)

// Kernel-specific abort
#define abort() BUG()

// Disable threading functions for kernel space
#define pthread_create(thread, attr, start_routine, arg) -1
#define pthread_join(thread, retval) -1
#define pthread_mutex_init(mutex, attr) -1
#define pthread_mutex_destroy(mutex) do {} while(0)
#define pthread_mutex_lock(mutex) -1
#define pthread_mutex_unlock(mutex) -1

// Disable dynamic loading
#define dlopen(filename, flag) NULL
#define dlsym(handle, symbol) NULL
#define dlclose(handle) do {} while(0)

// Kernel-specific memory mapping
#define mmap(addr, length, prot, flags, fd, offset) NULL
#define munmap(addr, length) -1

#endif // CONFIG_KERNEL_SHELL

#endif // _QUICKJS_KERNEL_ADAPTER_H
