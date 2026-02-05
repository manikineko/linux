// SPDX-License-Identifier: GPL-2.0-only
/*
 * Example Scriptable Kernel Module
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This module demonstrates how to make a kernel module scriptable
 * using the kernel shell scripting interface.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kernel_shell_module.h>
#include <linux/utsname.h>

MODULE_AUTHOR("Linux Kernel Developers");
MODULE_DESCRIPTION("Example Scriptable Kernel Module");
MODULE_LICENSE("GPL");

// Module private data
struct example_data {
    int counter;
    char message[256];
    bool debug_enabled;
    struct proc_dir_entry *proc_entry;
};

static struct example_data *ex_data;

// Module scripting operations
static int example_script_init(struct kernel_module_script_state *state)
{
    pr_info("example_module: Scripting interface initialized\n");
    return 0;
}

static void example_script_exit(struct kernel_module_script_state *state)
{
    pr_info("example_module: Scripting interface cleaned up\n");
}

static int example_script_suspend(struct kernel_module_script_state *state)
{
    pr_info("example_module: Suspended via scripting interface\n");
    return 0;
}

static int example_script_resume(struct kernel_module_script_state *state)
{
    pr_info("example_module: Resumed via scripting interface\n");
    return 0;
}

static int example_script_config(const char *key, const char *value)
{
    if (!key || !value) {
        return -EINVAL;
    }
    
    if (strcmp(key, "message") == 0) {
        strscpy(ex_data->message, value, sizeof(ex_data->message));
        pr_info("example_module: Message set to: %s\n", ex_data->message);
        return 0;
    } else if (strcmp(key, "debug") == 0) {
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            ex_data->debug_enabled = true;
        } else {
            ex_data->debug_enabled = false;
        }
        pr_info("example_module: Debug %s\n", ex_data->debug_enabled ? "enabled" : "disabled");
        return 0;
    } else if (strcmp(key, "counter") == 0) {
        int val;
        if (kstrtoint(value, 10, &val) == 0) {
            ex_data->counter = val;
            pr_info("example_module: Counter set to: %d\n", ex_data->counter);
            return 0;
        }
    }
    
    return -EINVAL;
}

static int example_script_status(char *output, size_t output_size)
{
    return snprintf(output, output_size,
                    "Example Module Status:\n"
                    "  Counter: %d\n"
                    "  Message: %s\n"
                    "  Debug: %s\n"
                    "  Process: %d (%s)\n",
                    ex_data->counter,
                    ex_data->message,
                    ex_data->debug_enabled ? "enabled" : "disabled",
                    current->pid, current->comm);
}

static int example_script_debug(const char *command, char *output, size_t output_size)
{
    if (!command) {
        return -EINVAL;
    }
    
    if (strcmp(command, "dump") == 0) {
        return snprintf(output, output_size,
                        "Module Debug Dump:\n"
                        "  ex_data = 0x%lx\n"
                        "  counter = %d\n"
                        "  message = \"%s\"\n"
                        "  debug_enabled = %s\n"
                        "  current_task = 0x%lx\n"
                        "  jiffies = %lu\n",
                        (unsigned long)ex_data,
                        ex_data->counter,
                        ex_data->message,
                        ex_data->debug_enabled ? "true" : "false",
                        (unsigned long)current,
                        jiffies);
    } else if (strcmp(command, "reset") == 0) {
        ex_data->counter = 0;
        strcpy(ex_data->message, "Reset message");
        ex_data->debug_enabled = false;
        return snprintf(output, output_size, "Module reset completed");
    } else if (strncmp(command, "echo ", 5) == 0) {
        return snprintf(output, output_size, "Echo: %s", command + 5);
    }
    
    return snprintf(output, output_size, "Unknown debug command: %s", command);
}

// Custom scripting commands
static int example_cmd_increment(const char *args, char *output, size_t output_size)
{
    int value = 1;
    
    if (args && strlen(args) > 0) {
        if (kstrtoint(args, 10, &value) != 0) {
            return snprintf(output, output_size, "Invalid number: %s", args);
        }
    }
    
    ex_data->counter += value;
    return snprintf(output, output_size, "Counter incremented by %d, new value: %d", value, ex_data->counter);
}

static int example_cmd_decrement(const char *args, char *output, size_t output_size)
{
    int value = 1;
    
    if (args && strlen(args) > 0) {
        if (kstrtoint(args, 10, &value) != 0) {
            return snprintf(output, output_size, "Invalid number: %s", args);
        }
    }
    
    ex_data->counter -= value;
    return snprintf(output, output_size, "Counter decremented by %d, new value: %d", value, ex_data->counter);
}

static int example_cmd_set_message(const char *args, char *output, size_t output_size)
{
    if (!args || strlen(args) == 0) {
        return snprintf(output, output_size, "Usage: set_message <message>");
    }
    
    strscpy(ex_data->message, args, sizeof(ex_data->message));
    return snprintf(output, output_size, "Message set to: %s", ex_data->message);
}

static int example_cmd_get_info(const char *args, char *output, size_t output_size)
{
    return snprintf(output, output_size,
                    "Module Information:\n"
                    "  Name: example_scriptable_module\n"
                    "  Version: 1.0\n"
                    "  Counter: %d\n"
                    "  Message: %s\n"
                    "  Debug: %s\n"
                    "  Kernel Version: %s\n"
                    "  Build Date: %s",
                    ex_data->counter,
                    ex_data->message,
                    ex_data->debug_enabled ? "enabled" : "disabled",
                    utsname()->release,
                    "2024-01-01");
}

// Proc file interface
static int example_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Example Scriptable Module\n");
    seq_printf(m, "Counter: %d\n", ex_data->counter);
    seq_printf(m, "Message: %s\n", ex_data->message);
    seq_printf(m, "Debug: %s\n", ex_data->debug_enabled ? "enabled" : "disabled");
    return 0;
}

static int example_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, example_proc_show, NULL);
}

static const struct proc_ops example_proc_ops = {
    .proc_open = example_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Module scripting operations structure
static const struct kernel_module_script_ops example_script_ops = {
    .init = example_script_init,
    .exit = example_script_exit,
    .suspend = example_script_suspend,
    .resume = example_script_resume,
    .config = example_script_config,
    .status = example_script_status,
    .debug = example_script_debug,
};

// Module initialization
static int __init example_module_init(void)
{
    int ret;
    
    pr_info("example_module: Loading scriptable example module\n");
    
    // Allocate private data
    ex_data = kzalloc(sizeof(*ex_data), GFP_KERNEL);
    if (!ex_data) {
        return -ENOMEM;
    }
    
    // Initialize default values
    ex_data->counter = 42;
    strcpy(ex_data->message, "Hello from scriptable module!");
    ex_data->debug_enabled = false;
    
    // Create proc entry
    ex_data->proc_entry = proc_create("example_scriptable", 0444, NULL, &example_proc_ops);
    if (!ex_data->proc_entry) {
        pr_warn("example_module: Failed to create proc entry\n");
    }
    
    // Register for scripting control
    ret = KERNEL_MODULE_SCRIPTING_REGISTER(THIS_MODULE, &example_script_ops);
    if (ret) {
        pr_err("example_module: Failed to register scripting interface: %d\n", ret);
        kfree(ex_data);
        return ret;
    }
    
    // Register custom scripting commands
    KERNEL_MODULE_SCRIPT_CMD_REGISTER(THIS_MODULE, "increment", 
                                     "Increment counter by specified amount", 
                                     example_cmd_increment);
    
    KERNEL_MODULE_SCRIPT_CMD_REGISTER(THIS_MODULE, "decrement", 
                                     "Decrement counter by specified amount", 
                                     example_cmd_decrement);
    
    KERNEL_MODULE_SCRIPT_CMD_REGISTER(THIS_MODULE, "set_message", 
                                     "Set module message", 
                                     example_cmd_set_message);
    
    KERNEL_MODULE_SCRIPT_CMD_REGISTER(THIS_MODULE, "get_info", 
                                     "Get detailed module information", 
                                     example_cmd_get_info);
    
    pr_info("example_module: Module loaded successfully\n");
    pr_info("example_module: Try 'module_cmd example_scriptable_module increment 5' in kernel shell\n");
    return 0;
}

// Module cleanup
static void __exit example_module_exit(void)
{
    pr_info("example_module: Unloading scriptable example module\n");
    
    // Unregister scripting commands
    KERNEL_MODULE_SCRIPT_CMD_UNREGISTER(THIS_MODULE, "increment");
    KERNEL_MODULE_SCRIPT_CMD_UNREGISTER(THIS_MODULE, "decrement");
    KERNEL_MODULE_SCRIPT_CMD_UNREGISTER(THIS_MODULE, "set_message");
    KERNEL_MODULE_SCRIPT_CMD_UNREGISTER(THIS_MODULE, "get_info");
    
    // Unregister scripting interface
    KERNEL_MODULE_SCRIPTING_UNREGISTER(THIS_MODULE);
    
    // Remove proc entry
    if (ex_data->proc_entry) {
        proc_remove(ex_data->proc_entry);
    }
    
    // Free private data
    kfree(ex_data);
    
    pr_info("example_module: Module unloaded successfully\n");
}

module_init(example_module_init);
module_exit(example_module_exit);

// Module parameters
static int debug_param = 0;
module_param(debug_param, int, 0644);
MODULE_PARM_DESC(debug_param, "Enable debug mode");

static char message_param[256] = "Default parameter message";
module_param_string(message_param, message_param, sizeof(message_param), 0644);
MODULE_PARM_DESC(message_param, "Default message");
