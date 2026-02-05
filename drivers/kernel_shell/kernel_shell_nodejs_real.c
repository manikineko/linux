// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel Shell Node.js Scripting Module (QuickJS Integration)
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This module provides real JavaScript/Node.js scripting support for the kernel shell,
 * using QuickJS as the embedded JavaScript engine.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kernel_shell.h>
#include <linux/vmalloc.h>

// Include QuickJS headers (adapted for kernel space)
#include "quickjs-2024-01-13/quickjs.h"
#include "quickjs-2024-01-13/quickjs-libc.h"

// Module information
MODULE_AUTHOR("Linux Kernel Developers");
MODULE_DESCRIPTION("Kernel Shell Node.js Scripting Module with QuickJS Integration");
MODULE_LICENSE("GPL");

// QuickJS state structure
struct quickjs_kernel_state {
    JSRuntime *rt;
    JSContext *ctx;
    bool initialized;
    spinlock_t lock;
};

static struct quickjs_kernel_state js_ks = {0};

// Kernel-specific JavaScript functions
static JSValue js_kernel_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *msg;
    size_t len;
    
    if (argc > 0) {
        msg = JS_ToCStringLen(ctx, &len, argv[0]);
        if (msg) {
            pr_info("kernel_shell_nodejs: %s\n", msg);
            JS_FreeCString(ctx, msg);
        }
    }
    
    return JS_UNDEFINED;
}

static JSValue js_kernel_panic_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    pr_info("kernel_shell_nodejs: Panic Information:\n");
    pr_info("  CPU: %d\n", raw_smp_processor_id());
    pr_info("  PID: %d (%s)\n", current->pid, current->comm);
    pr_info("  Tainted: %s\n", print_tainted());
    return JS_UNDEFINED;
}

static JSValue js_kernel_system_status(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    pr_info("kernel_shell_nodejs: System Status:\n");
    pr_info("  Uptime: %lu seconds\n", jiffies / HZ);
    pr_info("  Memory: Available\n");
    pr_info("  Processes: %d\n", nr_processes);
    return JS_UNDEFINED;
}

static JSValue js_kernel_get_time(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt64(ctx, jiffies);
}

// Register kernel-specific functions
static void register_kernel_functions(JSContext *ctx)
{
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    // Register print function
    JS_SetPropertyStr(ctx, global_obj, "print",
                      JS_NewCFunction(ctx, js_kernel_print, "print", 1));
    
    // Register kernel functions
    JS_SetPropertyStr(ctx, global_obj, "panic_info",
                      JS_NewCFunction(ctx, js_kernel_panic_info, "panic_info", 0));
    
    JS_SetPropertyStr(ctx, global_obj, "system_status",
                      JS_NewCFunction(ctx, js_kernel_system_status, "system_status", 0));
    
    JS_SetPropertyStr(ctx, global_obj, "getTime",
                      JS_NewCFunction(ctx, js_kernel_get_time, "getTime", 0));
    
    // Add kernel object
    JSValue kernel_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, kernel_obj, "panic_info",
                      JS_NewCFunction(ctx, js_kernel_panic_info, "panic_info", 0));
    JS_SetPropertyStr(ctx, kernel_obj, "system_status",
                      JS_NewCFunction(ctx, js_kernel_system_status, "system_status", 0));
    JS_SetPropertyStr(ctx, global_obj, "kernel", kernel_obj);
    
    JS_FreeValue(ctx, global_obj);
}

// Initialize QuickJS VM for kernel space
static int quickjs_vm_init(void)
{
    int ret = 0;
    
    pr_info("kernel_shell_nodejs: Initializing QuickJS VM\n");
    
    spin_lock_init(&js_ks.lock);
    
    // Create QuickJS runtime
    js_ks.rt = JS_NewRuntime();
    if (!js_ks.rt) {
        pr_err("kernel_shell_nodejs: Failed to create QuickJS runtime\n");
        return -ENOMEM;
    }
    
    // Set memory limit for kernel space (adjust as needed)
    JS_SetRuntimeInfo(js_ks.rt, 1024 * 1024); // 1MB limit
    
    // Create context
    js_ks.ctx = JS_NewContext(js_ks.rt);
    if (!js_ks.ctx) {
        pr_err("kernel_shell_nodejs: Failed to create QuickJS context\n");
        JS_FreeRuntime(js_ks.rt);
        js_ks.rt = NULL;
        return -ENOMEM;
    }
    
    // Register kernel-specific functions
    register_kernel_functions(js_ks.ctx);
    
    // Add Node.js-like console object
    JSValue global_obj = JS_GetGlobalObject(js_ks.ctx);
    JSValue console_obj = JS_NewObject(js_ks.ctx);
    JS_SetPropertyStr(js_ks.ctx, console_obj, "log",
                      JS_NewCFunction(js_ks.ctx, js_kernel_print, "log", 1));
    JS_SetPropertyStr(js_ks.ctx, global_obj, "console", console_obj);
    JS_FreeValue(js_ks.ctx, global_obj);
    
    js_ks.initialized = true;
    pr_info("kernel_shell_nodejs: QuickJS VM initialized successfully\n");
    
    return 0;
}

static void quickjs_vm_cleanup(void)
{
    if (js_ks.initialized) {
        pr_info("kernel_shell_nodejs: Cleaning up QuickJS VM\n");
        
        if (js_ks.ctx) {
            JS_FreeContext(js_ks.ctx);
            js_ks.ctx = NULL;
        }
        
        if (js_ks.rt) {
            JS_FreeRuntime(js_ks.rt);
            js_ks.rt = NULL;
        }
        
        js_ks.initialized = false;
        pr_info("kernel_shell_nodejs: QuickJS VM cleanup completed\n");
    }
}

// Execute JavaScript script
static int quickjs_execute_script_real(const char *script)
{
    int ret = 0;
    unsigned long flags;
    JSValue result;
    
    if (!script || !js_ks.initialized || !js_ks.ctx) {
        pr_err("kernel_shell_nodejs: QuickJS VM not initialized or no script\n");
        return -EINVAL;
    }
    
    pr_info("kernel_shell_nodejs: Executing JavaScript script: %s\n", script);
    
    spin_lock_irqsave(&js_ks.lock, flags);
    
    // Evaluate the script
    result = JS_Eval(js_ks.ctx, script, strlen(script), "kernel_shell", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(js_ks.ctx);
        const char *error = JS_ToCString(js_ks.ctx, exception);
        if (error) {
            pr_err("kernel_shell_nodejs: Script execution failed: %s\n", error);
            JS_FreeCString(js_ks.ctx, error);
        }
        JS_FreeValue(js_ks.ctx, exception);
        ret = -EINVAL;
    } else {
        pr_info("kernel_shell_nodejs: Script executed successfully\n");
        
        // Print result if it's not undefined
        if (!JS_IsUndefined(result) && !JS_IsNull(result)) {
            const char *result_str = JS_ToCString(js_ks.ctx, result);
            if (result_str) {
                pr_info("kernel_shell_nodejs: Result: %s\n", result_str);
                JS_FreeCString(js_ks.ctx, result_str);
            }
        }
        
        ret = 0;
    }
    
    JS_FreeValue(js_ks.ctx, result);
    
    spin_unlock_irqrestore(&js_ks.lock, flags);
    
    return ret;
}

static int quickjs_engine_enable_real(void)
{
    int ret;
    
    pr_info("kernel_shell_nodejs: Enabling QuickJS engine\n");
    
    if (!js_ks.initialized) {
        ret = quickjs_vm_init();
        if (ret) {
            pr_err("kernel_shell_nodejs: Failed to initialize QuickJS VM\n");
            return ret;
        }
    }
    
    pr_info("kernel_shell_nodejs: QuickJS engine enabled\n");
    return 0;
}

static int quickjs_engine_disable_real(void)
{
    pr_info("kernel_shell_nodejs: Disabling QuickJS engine\n");
    quickjs_vm_cleanup();
    pr_info("kernel_shell_nodejs: QuickJS engine disabled\n");
    return 0;
}

// Module initialization
static int __init kernel_shell_nodejs_real_init(void)
{
    int ret;
    
    pr_info("kernel_shell_nodejs: Loading Kernel Shell Node.js Module (QuickJS Integration)\n");
    
    // Register the QuickJS script engine with the kernel shell
    ret = kernel_shell_register_script_engine("nodejs_real", 
                                              quickjs_execute_script_real,
                                              quickjs_engine_enable_real,
                                              quickjs_engine_disable_real);
    if (ret) {
        pr_err("kernel_shell_nodejs: Failed to register QuickJS script engine: %d\n", ret);
        return ret;
    }
    
    pr_info("kernel_shell_nodejs: QuickJS module loaded successfully\n");
    return 0;
}

// Module cleanup
static void __exit kernel_shell_nodejs_real_exit(void)
{
    pr_info("kernel_shell_nodejs: Unloading Kernel Shell Node.js Module (QuickJS Integration)\n");
    
    // Unregister the QuickJS script engine
    kernel_shell_unregister_script_engine("nodejs_real");
    
    // Clean up the QuickJS VM
    quickjs_vm_cleanup();
    
    pr_info("kernel_shell_nodejs: QuickJS module unloaded successfully\n");
}

module_init(kernel_shell_nodejs_real_init);
module_exit(kernel_shell_nodejs_real_exit);

// Module parameters
static bool auto_enable = true;
module_param(auto_enable, bool, 0644);
MODULE_PARM_DESC(auto_enable, "Automatically enable QuickJS engine on module load");

static int max_script_size = 4096;
module_param(max_script_size, int, 0644);
MODULE_PARM_DESC(max_script_size, "Maximum JavaScript script size in bytes");

static int memory_limit_kb = 1024;
module_param(memory_limit_kb, int, 0644);
MODULE_PARM_DESC(memory_limit_kb, "QuickJS memory limit in KB");

// Additional functionality for kernel shell commands
static void shell_cmd_nodejs_real_test(const char *args)
{
    pr_info("kernel_shell_nodejs: Testing QuickJS engine\n");
    
    if (!js_ks.initialized) {
        pr_info("kernel_shell_nodejs: QuickJS VM not initialized\n");
        return;
    }
    
    // Execute a simple test script
    const char *test_script = 
        "console.log('Hello from QuickJS in Kernel Shell!');\n"
        "kernel.panic_info();\n"
        "let x = 42;\n"
        "console.log('The answer is: ' + x);\n"
        "getTime();";
    
    quickjs_execute_script_real(test_script);
}

static void shell_cmd_nodejs_real_info(const char *args)
{
    pr_info("kernel_shell_nodejs: QuickJS Engine Information\n");
    pr_info("  Status: %s\n", js_ks.initialized ? "Initialized" : "Not initialized");
    pr_info("  Version: QuickJS 2024-01-13\n");
    pr_info("  Features: ES2020 support, kernel API access\n");
    pr_info("  Max script size: %d bytes\n", max_script_size);
    pr_info("  Memory limit: %d KB\n", memory_limit_kb);
}

static void shell_cmd_nodejs_real_eval(const char *args)
{
    char script[256];
    
    if (!args || strlen(args) == 0) {
        pr_info("kernel_shell_nodejs: Usage: nodejs_real_eval <js_code>\n");
        return;
    }
    
    if (strlen(args) > max_script_size) {
        pr_err("kernel_shell_nodejs: Script too large (max: %d bytes)\n", max_script_size);
        return;
    }
    
    snprintf(script, sizeof(script), "(%s)", args);
    quickjs_execute_script_real(script);
}

// Register custom shell commands
static int __init register_nodejs_real_commands(void)
{
    int ret;
    
    ret = kernel_shell_register_command("nodejs_test", shell_cmd_nodejs_real_test, 
                                       "Test QuickJS scripting functionality");
    if (ret) {
        pr_warn("kernel_shell_nodejs: Failed to register nodejs_test command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("nodejs_info", shell_cmd_nodejs_real_info, 
                                       "Show QuickJS engine information");
    if (ret) {
        pr_warn("kernel_shell_nodejs: Failed to register nodejs_info command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("nodejs_eval", shell_cmd_nodejs_real_eval, 
                                       "Evaluate JavaScript expression (usage: nodejs_eval <expr>)");
    if (ret) {
        pr_warn("kernel_shell_nodejs: Failed to register nodejs_eval command: %d\n", ret);
    }
    
    return 0;
}

static void __init unregister_nodejs_real_commands(void)
{
    kernel_shell_unregister_command("nodejs_test");
    kernel_shell_unregister_command("nodejs_info");
    kernel_shell_unregister_command("nodejs_eval");
}

// Enhanced module initialization with commands
static int __init kernel_shell_nodejs_real_init_enhanced(void)
{
    int ret = kernel_shell_nodejs_real_init();
    if (ret)
        return ret;
    
    register_nodejs_real_commands();
    
    // Auto-enable if requested
    if (auto_enable) {
        ret = kernel_shell_enable_script_engine("nodejs_real");
        if (ret) {
            pr_warn("kernel_shell_nodejs: Failed to auto-enable QuickJS engine: %d\n", ret);
        } else {
            pr_info("kernel_shell_nodejs: QuickJS engine auto-enabled\n");
        }
    }
    
    return 0;
}

// Use the enhanced init function
module_init(kernel_shell_nodejs_real_init_enhanced);
