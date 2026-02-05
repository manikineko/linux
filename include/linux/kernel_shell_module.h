/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Kernel Shell Module Scripting Interface
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This header provides the interface for kernel modules to be
 * controlled and configured via scripting languages.
 */

#ifndef _LINUX_KERNEL_SHELL_MODULE_H
#define _LINUX_KERNEL_SHELL_MODULE_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kernel_shell.h>

#ifdef CONFIG_KERNEL_SHELL_SCRIPTING

// Module scripting command structure
struct kernel_module_script_cmd {
    char name[64];
    char description[128];
    int (*execute)(const char *args, char *output, size_t output_size);
    struct list_head list;
};

// Module scripting state
struct kernel_module_script_state {
    struct module *module;
    struct list_head commands;
    struct list_head list;  // For linking into global list
    struct mutex lock;
    bool scripting_enabled;
    char module_name[MODULE_NAME_LEN];
};

// Module scripting operations
struct kernel_module_script_ops {
    int (*init)(struct kernel_module_script_state *state);
    void (*exit)(struct kernel_module_script_state *state);
    int (*suspend)(struct kernel_module_script_state *state);
    int (*resume)(struct kernel_module_script_state *state);
    int (*config)(const char *key, const char *value);
    int (*status)(char *output, size_t output_size);
    int (*debug)(const char *command, char *output, size_t output_size);
};

// Register a module for scripting control
int kernel_module_register_scripting(struct module *mod,
                                     const struct kernel_module_script_ops *ops);

// Unregister module scripting
void kernel_module_unregister_scripting(struct module *mod);

// Register a scripting command for a module
int kernel_module_register_script_cmd(struct module *mod,
                                      const char *name,
                                      const char *description,
                                      int (*execute)(const char *args, char *output, size_t output_size));

// Unregister a scripting command
void kernel_module_unregister_script_cmd(struct module *mod, const char *name);

// Execute a module script command
int kernel_module_execute_script_cmd(struct module *mod,
                                    const char *cmd,
                                    const char *args,
                                    char *output,
                                    size_t output_size);

// Get module scripting state
struct kernel_module_script_state *kernel_module_get_script_state(struct module *mod);

// Helper macros for module scripting
#define KERNEL_MODULE_SCRIPTING_REGISTER(mod, ops) \
    kernel_module_register_scripting(mod, ops)

#define KERNEL_MODULE_SCRIPTING_UNREGISTER(mod) \
    kernel_module_unregister_scripting(mod)

#define KERNEL_MODULE_SCRIPT_CMD_REGISTER(mod, name, desc, func) \
    kernel_module_register_script_cmd(mod, name, desc, func)

#define KERNEL_MODULE_SCRIPT_CMD_UNREGISTER(mod, name) \
    kernel_module_unregister_script_cmd(mod, name)

// Standard module scripting commands
#define MODULE_SCRIPT_CMD_STATUS    "status"
#define MODULE_SCRIPT_CMD_CONFIG    "config"
#define MODULE_SCRIPT_CMD_DEBUG     "debug"
#define MODULE_SCRIPT_CMD_SUSPEND   "suspend"
#define MODULE_SCRIPT_CMD_RESUME    "resume"
#define MODULE_SCRIPT_CMD_INIT      "init"
#define MODULE_SCRIPT_CMD_EXIT      "exit"

// Scripting context for modules
struct kernel_module_script_context {
    struct module *module;
    const char *engine;  // "lua" or "nodejs"
    void *engine_state;
    char last_error[256];
    bool debug_enabled;
};

// Execute script in module context
int kernel_module_execute_script(struct module *mod,
                                const char *engine,
                                const char *script,
                                char *output,
                                size_t output_size);

// Get list of scriptable modules
int kernel_module_get_scriptable_modules(char *output, size_t output_size);

#else // CONFIG_KERNEL_SHELL_SCRIPTING

// Stub functions when scripting is disabled
static inline int kernel_module_register_scripting(struct module *mod,
                                                   const struct kernel_module_script_ops *ops)
{
    return -ENODEV;
}

static inline void kernel_module_unregister_scripting(struct module *mod)
{
}

static inline int kernel_module_register_script_cmd(struct module *mod,
                                                   const char *name,
                                                   const char *description,
                                                   int (*execute)(const char *args, char *output, size_t output_size))
{
    return -ENODEV;
}

static inline void kernel_module_unregister_script_cmd(struct module *mod, const char *name)
{
}

static inline int kernel_module_execute_script_cmd(struct module *mod,
                                                   const char *cmd,
                                                   const char *args,
                                                   char *output,
                                                   size_t output_size)
{
    return -ENODEV;
}

static inline struct kernel_module_script_state *kernel_module_get_script_state(struct module *mod)
{
    return NULL;
}

static inline int kernel_module_execute_script(struct module *mod,
                                               const char *engine,
                                               const char *script,
                                               char *output,
                                               size_t output_size)
{
    return -ENODEV;
}

static inline int kernel_module_get_scriptable_modules(char *output, size_t output_size)
{
    return -ENODEV;
}

#endif // CONFIG_KERNEL_SHELL_SCRIPTING

#endif // _LINUX_KERNEL_SHELL_MODULE_H
