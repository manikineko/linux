/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * QuickJS Kernel Space Patches
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This file provides kernel space implementations for QuickJS functions
 * that are not compatible with the kernel environment.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/random.h>
#include <linux/delay.h>
#include "quickjs_kernel_adapter.h"

// Kernel space implementations for QuickJS functions

// Memory allocation with tracking
#ifdef DEBUG_QUICKJS_MEMORY
static atomic_t qjs_alloc_count = ATOMIC_INIT(0);
static atomic_t qjs_alloc_bytes = ATOMIC_INIT(0);

void *qjs_malloc(size_t size)
{
    void *ptr = kmalloc(size, GFP_KERNEL);
    if (ptr) {
        atomic_inc(&qjs_alloc_count);
        atomic_add(size, &qjs_alloc_bytes);
        pr_debug("QuickJS malloc: %p (%zu bytes, total: %d allocations, %d bytes)\n",
                ptr, size, atomic_read(&qjs_alloc_count), atomic_read(&qjs_alloc_bytes));
    }
    return ptr;
}

void qjs_free(void *ptr)
{
    if (ptr) {
        atomic_dec(&qjs_alloc_count);
        pr_debug("QuickJS free: %p (total: %d allocations)\n", ptr, atomic_read(&qjs_alloc_count));
        kfree(ptr);
    }
}

void *qjs_realloc(void *ptr, size_t size)
{
    void *new_ptr = krealloc(ptr, size, GFP_KERNEL);
    if (new_ptr) {
        pr_debug("QuickJS realloc: %p -> %p (%zu bytes)\n", ptr, new_ptr, size);
    }
    return new_ptr;
}
#else
// Non-debug versions
void *qjs_malloc(size_t size)
{
    return kmalloc(size, GFP_KERNEL);
}

void qjs_free(void *ptr)
{
    kfree(ptr);
}

void *qjs_realloc(void *ptr, size_t size)
{
    return krealloc(ptr, size, GFP_KERNEL);
}
#endif

// Kernel-specific file operations (disabled for safety)
FILE *qjs_fopen(const char *filename, const char *mode)
{
    // File operations disabled in kernel space for safety
    return NULL;
}

int qjs_fclose(FILE *fp)
{
    return 0;
}

size_t qjs_fread(void *ptr, size_t size, size_t n, FILE *fp)
{
    return 0;
}

size_t qjs_fwrite(const void *ptr, size_t size, size_t n, FILE *fp)
{
    return 0;
}

int qjs_fseek(FILE *fp, long offset, int whence)
{
    return -1;
}

long qjs_ftell(FILE *fp)
{
    return -1;
}

int qjs_fflush(FILE *fp)
{
    return 0;
}

// Kernel-specific time functions
time_t qjs_time(time_t *tloc)
{
    time_t result = jiffies / HZ;
    if (tloc) {
        *tloc = result;
    }
    return result;
}

clock_t qjs_clock(void)
{
    return jiffies;
}

// Kernel-specific sleep function
void qjs_sleep(unsigned int seconds)
{
    msleep(seconds * 1000);
}

void qjs_usleep(useconds_t useconds)
{
    usleep_range(useconds, useconds + 1000);
}

// Kernel-specific random number generator
int qjs_rand(void)
{
    return get_random_u32();
}

void qjs_srand(unsigned int seed)
{
    // Seed is handled by kernel's random number generator
}

// Kernel-specific environment functions
char *qjs_getenv(const char *name)
{
    // Environment variables not available in kernel space
    return NULL;
}

int qjs_setenv(const char *name, const char *value, int overwrite)
{
    // Environment variables not available in kernel space
    return -1;
}

int qjs_unsetenv(const char *name)
{
    // Environment variables not available in kernel space
    return -1;
}

// Kernel-specific system functions
int qjs_system(const char *command)
{
    // System commands disabled in kernel space for safety
    return -1;
}

// Kernel-specific exec functions (disabled for safety)
int qjs_execve(const char *filename, char *const argv[], char *const envp[])
{
    return -1;
}

int qjs_execvp(const char *file, char *const argv[])
{
    return -1;
}

// Kernel-specific process functions (disabled for safety)
pid_t qjs_fork(void)
{
    return -1;
}

pid_t qjs_getpid(void)
{
    return current->pid;
}

pid_t qjs_getppid(void)
{
    return current->real_parent->pid;
}

int qjs_kill(pid_t pid, int sig)
{
    return -1; // Process killing disabled for safety
}

// Kernel-specific threading functions (disabled for safety)
int qjs_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                      void *(*start_routine)(void *), void *arg)
{
    return -1;
}

int qjs_pthread_join(pthread_t thread, void **retval)
{
    return -1;
}

int qjs_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    return -1;
}

int qjs_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return 0;
}

int qjs_pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return -1;
}

int qjs_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return -1;
}

// Kernel-specific dynamic loading (disabled for safety)
void *qjs_dlopen(const char *filename, int flags)
{
    return NULL;
}

void *qjs_dlsym(void *handle, const char *symbol)
{
    return NULL;
}

int qjs_dlclose(void *handle)
{
    return 0;
}

char *qjs_dlerror(void)
{
    return "Dynamic loading disabled in kernel space";
}

// Kernel-specific memory mapping
void *qjs_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return NULL;
}

int qjs_munmap(void *addr, size_t length)
{
    return -1;
}

// Kernel-specific socket functions (disabled for safety)
int qjs_socket(int domain, int type, int protocol)
{
    return -1;
}

int qjs_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return -1;
}

int qjs_listen(int sockfd, int backlog)
{
    return -1;
}

int qjs_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return -1;
}

int qjs_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return -1;
}

ssize_t qjs_send(int sockfd, const void *buf, size_t len, int flags)
{
    return -1;
}

ssize_t qjs_recv(int sockfd, void *buf, size_t len, int flags)
{
    return -1;
}

// Kernel-specific string operations
char *qjs_strdup(const char *s)
{
    return kstrdup(s, GFP_KERNEL);
}

char *qjs_strndup(const char *s, size_t n)
{
    return kstrndup(s, n, GFP_KERNEL);
}

// Kernel-specific math functions
double qjs_strtod(const char *nptr, char **endptr)
{
    // Simple implementation - in a real scenario, you'd want proper parsing
    double result = 0.0;
    if (nptr) {
        // Use kernel's simple_strtoull for basic parsing
        u64 val = simple_strtoull(nptr, endptr, 10);
        result = (double)val;
    }
    return result;
}

// Kernel-specific exit functions
void qjs_exit(int status)
{
    pr_err("QuickJS: exit called with status %d\n", status);
    // Don't actually exit the kernel
}

void qjs_abort(void)
{
    BUG();
}

// Kernel-specific signal functions (disabled for safety)
void (*qjs_signal(int signum, void (*handler)(int)))(int)
{
    return SIG_ERR;
}

int qjs_raise(int sig)
{
    return -1;
}

// Kernel-specific terminal functions (disabled for safety)
int qjs_isatty(int fd)
{
    return 0;
}

char *qjs_ttyname(int fd)
{
    return NULL;
}

// Kernel-specific user/group functions (disabled for safety)
uid_t qjs_getuid(void)
{
    return 0;
}

gid_t qjs_getgid(void)
{
    return 0;
}

uid_t qjs_geteuid(void)
{
    return 0;
}

gid_t qjs_getegid(void)
{
    return 0;
}

// Kernel-specific directory functions (disabled for safety)
DIR *qjs_opendir(const char *name)
{
    return NULL;
}

int qjs_closedir(DIR *dirp)
{
    return -1;
}

struct dirent *qjs_readdir(DIR *dirp)
{
    return NULL;
}

void qjs_rewinddir(DIR *dirp)
{
}

// Kernel-specific stat functions (disabled for safety)
int qjs_stat(const char *pathname, struct stat *statbuf)
{
    return -1;
}

int qjs_fstat(int fd, struct stat *statbuf)
{
    return -1;
}

// Kernel-specific unlink function
int qjs_unlink(const char *pathname)
{
    return -1;
}

// Kernel-specific rename function
int qjs_rename(const char *oldpath, const char *newpath)
{
    return -1;
}
