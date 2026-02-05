/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Kernel Shell Interface
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This header provides the interface for kernel shell functionality,
 * including command registration and script engine management.
 */

#ifndef _LINUX_KERNEL_SHELL_H
#define _LINUX_KERNEL_SHELL_H

#include <linux/types.h>
#include <linux/list.h>

#ifdef CONFIG_KERNEL_SHELL

/**
 * kernel_shell_register_command - Register a new shell command
 * @name: Command name
 * @func: Function to execute for this command
 * @help: Help text for the command
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_register_command(const char *name, void (*func)(const char *), const char *help);

/**
 * kernel_shell_unregister_command - Unregister a shell command
 * @name: Command name to unregister
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_unregister_command(const char *name);

#ifdef CONFIG_KERNEL_SHELL_SCRIPTING
/**
 * kernel_shell_register_script_engine - Register a new script engine
 * @name: Engine name
 * @execute: Function to execute scripts
 * @enable: Function to enable the engine
 * @disable: Function to disable the engine
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_register_script_engine(const char *name, 
                                        int (*execute)(const char *),
                                        int (*enable)(void),
                                        int (*disable)(void));

/**
 * kernel_shell_unregister_script_engine - Unregister a script engine
 * @name: Engine name to unregister
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_unregister_script_engine(const char *name);

/**
 * kernel_shell_enable_script_engine - Enable a script engine
 * @name: Engine name to enable
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_enable_script_engine(const char *name);

/**
 * kernel_shell_disable_script_engine - Disable a script engine
 * @name: Engine name to disable
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_disable_script_engine(const char *name);
#endif // CONFIG_KERNEL_SHELL_SCRIPTING

#else // CONFIG_KERNEL_SHELL

// Stub functions when kernel shell is disabled
static inline int kernel_shell_register_command(const char *name, void (*func)(const char *), const char *help)
{
    return -ENODEV;
}

static inline int kernel_shell_unregister_command(const char *name)
{
    return -ENODEV;
}

#ifdef CONFIG_KERNEL_SHELL_SCRIPTING
static inline int kernel_shell_register_script_engine(const char *name, 
                                                    int (*execute)(const char *),
                                                    int (*enable)(void),
                                                    int (*disable)(void))
{
    return -ENODEV;
}

static inline int kernel_shell_unregister_script_engine(const char *name)
{
    return -ENODEV;
}

static inline int kernel_shell_enable_script_engine(const char *name)
{
    return -ENODEV;
}

static inline int kernel_shell_disable_script_engine(const char *name)
{
    return -ENODEV;
}
#endif // CONFIG_KERNEL_SHELL_SCRIPTING

#endif // CONFIG_KERNEL_SHELL

#endif // _LINUX_KERNEL_SHELL_H
