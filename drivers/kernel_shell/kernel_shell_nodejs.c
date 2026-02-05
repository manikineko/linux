// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel Shell Node.js Scripting Module
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This module provides Node.js scripting support for the kernel shell,
 * allowing JavaScript scripts to be executed during panic conditions.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kernel_shell.h>

// Module information
MODULE_AUTHOR("Linux Kernel Developers");
MODULE_DESCRIPTION("Kernel Shell Node.js Scripting Module");
MODULE_LICENSE("GPL");

// Node.js state structure (simplified)
struct nodejs_state {
    void *js_runtime;  // In a real implementation, this would be a JS runtime
    bool initialized;
};

static struct nodejs_state nodejs_vm = {0};

// Forward declarations for Node.js functions
static int nodejs_vm_init(void);
static void nodejs_vm_cleanup(void);
static int nodejs_execute_script(const char *script);
static int nodejs_engine_enable(void);
static int nodejs_engine_disable(void);

// Simplified JavaScript script execution
static int nodejs_execute_script(const char *script)
{
    if (!script || !nodejs_vm.initialized) {
        pr_err("kernel_shell_nodejs: Node.js VM not initialized or no script\n");
        return -EINVAL;
    }

    pr_info("kernel_shell_nodejs: Executing JavaScript script: %s\n", script);
    
    // In a real implementation, we would:
    // 1. Use a JavaScript engine like QuickJS or JerryScript
    // 2. Execute the script in a sandboxed environment
    // 3. Handle JavaScript errors and exceptions
    // 4. Provide kernel-specific APIs to JavaScript
    // 5. Manage memory and security properly
    
    // For demonstration, we'll parse some basic JavaScript-like commands
    if (strstr(script, "console.log")) {
        pr_info("kernel_shell_nodejs: console.log command detected\n");
        // Extract what to log and log it
        char *start = strstr(script, "'");
        if (!start) start = strstr(script, "\"");
        if (start) {
            start++; // Skip opening quote
            char *end = strchr(start, '\'');
            if (!end) end = strchr(start, '\"');
            if (end) {
                int len = end - start;
                char *msg = kmalloc(len + 1, GFP_KERNEL);
                if (msg) {
                    strncpy(msg, start, len);
                    msg[len] = '\0';
                    pr_info("kernel_shell_nodejs: JS output: %s\n", msg);
                    kfree(msg);
                }
            }
        }
    }
    
    if (strstr(script, "kernel")) {
        pr_info("kernel_shell_nodejs: Kernel-specific command detected\n");
        // Handle kernel-specific JavaScript commands
        if (strstr(script, "getPanicInfo")) {
            pr_info("kernel_shell_nodejs: Providing panic information\n");
            // In a real implementation, we would provide access to panic data
        }
        if (strstr(script, "systemStatus")) {
            pr_info("kernel_shell_nodejs: Providing system status\n");
            // In a real implementation, we would provide system status
        }
    }

    // Handle function calls
    if (strstr(script, "function")) {
        pr_info("kernel_shell_nodejs: Function definition detected\n");
        // In a real implementation, we would parse and store functions
    }

    pr_info("kernel_shell_nodejs: Script execution completed\n");
    return 0;
}

static int nodejs_vm_init(void)
{
    pr_info("kernel_shell_nodejs: Initializing Node.js VM\n");
    
    // In a real implementation, we would:
    // 1. Initialize a JavaScript engine runtime
    // 2. Set up a global context
    // 3. Register kernel-specific functions and objects
    // 4. Configure security restrictions and sandbox
    // 5. Load essential Node.js modules (safe ones only)
    
    nodejs_vm.initialized = true;
    pr_info("kernel_shell_nodejs: Node.js VM initialized successfully\n");
    return 0;
}

static void nodejs_vm_cleanup(void)
{
    if (nodejs_vm.initialized) {
        pr_info("kernel_shell_nodejs: Cleaning up Node.js VM\n");
        
        // In a real implementation, we would:
        // 1. Free the JavaScript runtime and context
        // 2. Clean up any allocated resources
        // 3. Unregister kernel-specific functions
        
        nodejs_vm.initialized = false;
        pr_info("kernel_shell_nodejs: Node.js VM cleanup completed\n");
    }
}

static int nodejs_engine_enable(void)
{
    int ret;
    
    pr_info("kernel_shell_nodejs: Enabling Node.js engine\n");
    
    if (!nodejs_vm.initialized) {
        ret = nodejs_vm_init();
        if (ret) {
            pr_err("kernel_shell_nodejs: Failed to initialize Node.js VM\n");
            return ret;
        }
    }
    
    pr_info("kernel_shell_nodejs: Node.js engine enabled\n");
    return 0;
}

static int nodejs_engine_disable(void)
{
    pr_info("kernel_shell_nodejs: Disabling Node.js engine\n");
    nodejs_vm_cleanup();
    pr_info("kernel_shell_nodejs: Node.js engine disabled\n");
    return 0;
}

// Module initialization
static int __init kernel_shell_nodejs_init(void)
{
    int ret;
    
    pr_info("kernel_shell_nodejs: Loading Kernel Shell Node.js Module\n");
    
    // Register the Node.js script engine with the kernel shell
    ret = kernel_shell_register_script_engine("nodejs", 
                                              nodejs_execute_script,
                                              nodejs_engine_enable,
                                              nodejs_engine_disable);
    if (ret) {
        pr_err("kernel_shell_nodejs: Failed to register Node.js script engine: %d\n", ret);
        return ret;
    }
    
    pr_info("kernel_shell_nodejs: Module loaded successfully\n");
    return 0;
}

// Module cleanup
static void __exit kernel_shell_nodejs_exit(void)
{
    pr_info("kernel_shell_nodejs: Unloading Kernel Shell Node.js Module\n");
    
    // Unregister the Node.js script engine
    kernel_shell_unregister_script_engine("nodejs");
    
    // Clean up the Node.js VM
    nodejs_vm_cleanup();
    
    pr_info("kernel_shell_nodejs: Module unloaded successfully\n");
}

// Module parameters
static bool auto_enable = true;
module_param(auto_enable, bool, 0644);
MODULE_PARM_DESC(auto_enable, "Automatically enable Node.js engine on module load");

// Additional functionality for kernel shell commands
static void shell_cmd_nodejs_test(const char *args)
{
    pr_info("kernel_shell_nodejs: Testing Node.js engine\n");
    
    if (!nodejs_vm.initialized) {
        pr_info("kernel_shell_nodejs: Node.js VM not initialized\n");
        return;
    }
    
    // Execute a simple test script
    const char *test_script = "console.log('Hello from Node.js in Kernel Shell!');";
    nodejs_execute_script(test_script);
}

static void shell_cmd_nodejs_info(const char *args)
{
    pr_info("kernel_shell_nodejs: Node.js Engine Information\n");
    pr_info("  Status: %s\n", nodejs_vm.initialized ? "Initialized" : "Not initialized");
    pr_info("  Runtime: Simplified JavaScript Engine\n");
    pr_info("  Features: Basic JavaScript execution, kernel API access\n");
}

// Register custom shell commands
static int __init register_nodejs_commands(void)
{
    int ret;
    
    ret = kernel_shell_register_command("nodejs_test", shell_cmd_nodejs_test, 
                                       "Test Node.js scripting functionality");
    if (ret) {
        pr_warn("kernel_shell_nodejs: Failed to register nodejs_test command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("nodejs_info", shell_cmd_nodejs_info, 
                                       "Show Node.js engine information");
    if (ret) {
        pr_warn("kernel_shell_nodejs: Failed to register nodejs_info command: %d\n", ret);
    }
    
    return 0;
}

static void __init __maybe_unused unregister_nodejs_commands(void)
{
    kernel_shell_unregister_command("nodejs_test");
    kernel_shell_unregister_command("nodejs_info");
    kernel_shell_unregister_command("nodejs_eval");
}

// Enhanced module initialization with commands
static int __init kernel_shell_nodejs_init_enhanced(void)
{
    int ret = kernel_shell_nodejs_init();
    if (ret)
        return ret;
    
    register_nodejs_commands();
    
    // Auto-enable if requested
    if (auto_enable) {
        ret = kernel_shell_enable_script_engine("nodejs");
        if (ret) {
            pr_warn("kernel_shell_nodejs: Failed to auto-enable Node.js engine: %d\n", ret);
        } else {
            pr_info("kernel_shell_nodejs: Node.js engine auto-enabled\n");
        }
    }
    
    return 0;
}

// Use the enhanced init function
module_init(kernel_shell_nodejs_init_enhanced);
module_exit(kernel_shell_nodejs_exit);
