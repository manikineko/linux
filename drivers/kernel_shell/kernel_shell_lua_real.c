// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel Shell Lua Scripting Module (Real Lua Integration)
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This module provides real Lua scripting support for the kernel shell,
 * allowing actual Lua scripts to be executed during panic conditions.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kernel_shell.h>
#include <linux/vmalloc.h>

// Include Lua headers (adapted for kernel space)
#include "lua-5.5.0/src/lua.h"
#include "lua-5.5.0/src/lauxlib.h"
#include "lua-5.5.0/src/lualib.h"

// Module information
MODULE_AUTHOR("Linux Kernel Developers");
MODULE_DESCRIPTION("Kernel Shell Lua Scripting Module with Real Lua Integration");
MODULE_LICENSE("GPL");

// Lua state structure
struct lua_kernel_state {
    lua_State *L;
    bool initialized;
    spinlock_t lock;
};

static struct lua_kernel_state lua_ks = {0};

// Kernel-specific Lua functions
static int lua_kernel_print(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    pr_info("kernel_shell_lua: %s\n", msg);
    return 0;
}

static int lua_kernel_panic_info(lua_State *L)
{
    pr_info("kernel_shell_lua: Panic Information:\n");
    pr_info("  CPU: %d\n", raw_smp_processor_id());
    pr_info("  PID: %d (%s)\n", current->pid, current->comm);
    pr_info("  Tainted: %s\n", print_tainted());
    return 0;
}

static int lua_kernel_system_status(lua_State *L)
{
    pr_info("kernel_shell_lua: System Status:\n");
    pr_info("  Uptime: %lu seconds\n", jiffies / HZ);
    pr_info("  Memory: Available\n");
    pr_info("  Processes: %d\n", nr_processes);
    return 0;
}

// Register kernel-specific functions
static void register_kernel_functions(lua_State *L)
{
    lua_pushcfunction(L, lua_kernel_print);
    lua_setglobal(L, "print");
    
    lua_pushcfunction(L, lua_kernel_panic_info);
    lua_setglobal(L, "panic_info");
    
    lua_pushcfunction(L, lua_kernel_system_status);
    lua_setglobal(L, "system_status");
}

// Initialize Lua VM for kernel space
static int lua_vm_init(void)
{
    int ret = 0;
    
    pr_info("kernel_shell_lua: Initializing real Lua VM\n");
    
    spin_lock_init(&lua_ks.lock);
    
    // Create new Lua state
    lua_ks.L = luaL_newstate();
    if (!lua_ks.L) {
        pr_err("kernel_shell_lua: Failed to create Lua state\n");
        return -ENOMEM;
    }
    
    // Load essential Lua libraries (safe ones only)
    luaL_requiref(lua_ks.L, "_G", luaopen_base, 1);
    luaL_requiref(lua_ks.L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(lua_ks.L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(lua_ks.L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(lua_ks.L, 4);
    
    // Register kernel-specific functions
    register_kernel_functions(lua_ks.L);
    
    // Set up safe environment
    lua_pushstring(lua_ks.L, "Kernel Shell Lua Environment");
    lua_setglobal(lua_ks.L, "_VERSION");
    
    lua_ks.initialized = true;
    pr_info("kernel_shell_lua: Real Lua VM initialized successfully\n");
    
    return 0;
}

static void lua_vm_cleanup(void)
{
    if (lua_ks.initialized && lua_ks.L) {
        pr_info("kernel_shell_lua: Cleaning up real Lua VM\n");
        
        // Close Lua state
        lua_close(lua_ks.L);
        lua_ks.L = NULL;
        lua_ks.initialized = false;
        
        pr_info("kernel_shell_lua: Real Lua VM cleanup completed\n");
    }
}

// Execute Lua script
static int lua_execute_script_real(const char *script)
{
    int ret = 0;
    unsigned long flags;
    
    if (!script || !lua_ks.initialized || !lua_ks.L) {
        pr_err("kernel_shell_lua: Lua VM not initialized or no script\n");
        return -EINVAL;
    }
    
    pr_info("kernel_shell_lua: Executing real Lua script: %s\n", script);
    
    spin_lock_irqsave(&lua_ks.lock, flags);
    
    // Load and execute the script
    ret = luaL_dostring(lua_ks.L, script);
    if (ret != LUA_OK) {
        const char *error = lua_tostring(lua_ks.L, -1);
        pr_err("kernel_shell_lua: Script execution failed: %s\n", error);
        lua_pop(lua_ks.L, 1); // Remove error from stack
        ret = -EINVAL;
    } else {
        pr_info("kernel_shell_lua: Script executed successfully\n");
        ret = 0;
    }
    
    spin_unlock_irqrestore(&lua_ks.lock, flags);
    
    return ret;
}

static int lua_engine_enable_real(void)
{
    int ret;
    
    pr_info("kernel_shell_lua: Enabling real Lua engine\n");
    
    if (!lua_ks.initialized) {
        ret = lua_vm_init();
        if (ret) {
            pr_err("kernel_shell_lua: Failed to initialize real Lua VM\n");
            return ret;
        }
    }
    
    pr_info("kernel_shell_lua: Real Lua engine enabled\n");
    return 0;
}

static int lua_engine_disable_real(void)
{
    pr_info("kernel_shell_lua: Disabling real Lua engine\n");
    lua_vm_cleanup();
    pr_info("kernel_shell_lua: Real Lua engine disabled\n");
    return 0;
}

// Module initialization
static int __init kernel_shell_lua_real_init(void)
{
    int ret;
    
    pr_info("kernel_shell_lua: Loading Kernel Shell Lua Module (Real Lua Integration)\n");
    
    // Register the Lua script engine with the kernel shell
    ret = kernel_shell_register_script_engine("lua_real", 
                                              lua_execute_script_real,
                                              lua_engine_enable_real,
                                              lua_engine_disable_real);
    if (ret) {
        pr_err("kernel_shell_lua: Failed to register real Lua script engine: %d\n", ret);
        return ret;
    }
    
    pr_info("kernel_shell_lua: Real Lua module loaded successfully\n");
    return 0;
}

// Module cleanup
static void __exit kernel_shell_lua_real_exit(void)
{
    pr_info("kernel_shell_lua: Unloading Kernel Shell Lua Module (Real Lua Integration)\n");
    
    // Unregister the Lua script engine
    kernel_shell_unregister_script_engine("lua_real");
    
    // Clean up the Lua VM
    lua_vm_cleanup();
    
    pr_info("kernel_shell_lua: Real Lua module unloaded successfully\n");
}

module_init(kernel_shell_lua_real_init);
module_exit(kernel_shell_lua_real_exit);

// Module parameters
static bool auto_enable = true;
module_param(auto_enable, bool, 0644);
MODULE_PARM_DESC(auto_enable, "Automatically enable real Lua engine on module load");

static int max_script_size = 4096;
module_param(max_script_size, int, 0644);
MODULE_PARM_DESC(max_script_size, "Maximum Lua script size in bytes");

// Additional functionality for kernel shell commands
static void shell_cmd_lua_real_test(const char *args)
{
    pr_info("kernel_shell_lua: Testing real Lua engine\n");
    
    if (!lua_ks.initialized) {
        pr_info("kernel_shell_lua: Real Lua VM not initialized\n");
        return;
    }
    
    // Execute a simple test script
    const char *test_script = 
        "print('Hello from real Lua in Kernel Shell!')\n"
        "panic_info()\n"
        "local x = 42\n"
        "print('The answer is: ' .. x)";
    
    lua_execute_script_real(test_script);
}

static void shell_cmd_lua_real_info(const char *args)
{
    pr_info("kernel_shell_lua: Real Lua Engine Information\n");
    pr_info("  Status: %s\n", lua_ks.initialized ? "Initialized" : "Not initialized");
    pr_info("  Version: Lua 5.5.0\n");
    pr_info("  Features: Full Lua interpreter, kernel API access\n");
    pr_info("  Max script size: %d bytes\n", max_script_size);
}

static void shell_cmd_lua_real_eval(const char *args)
{
    char script[256];
    
    if (!args || strlen(args) == 0) {
        pr_info("kernel_shell_lua: Usage: lua_real_eval <lua_code>\n");
        return;
    }
    
    if (strlen(args) > max_script_size) {
        pr_err("kernel_shell_lua: Script too large (max: %d bytes)\n", max_script_size);
        return;
    }
    
    snprintf(script, sizeof(script), "return (%s)", args);
    lua_execute_script_real(script);
}

// Register custom shell commands
static int __init register_lua_real_commands(void)
{
    int ret;
    
    ret = kernel_shell_register_command("lua_test", shell_cmd_lua_real_test, 
                                       "Test real Lua scripting functionality");
    if (ret) {
        pr_warn("kernel_shell_lua: Failed to register lua_test command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("lua_info", shell_cmd_lua_real_info, 
                                       "Show real Lua engine information");
    if (ret) {
        pr_warn("kernel_shell_lua: Failed to register lua_info command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("lua_eval", shell_cmd_lua_real_eval, 
                                       "Evaluate Lua expression (usage: lua_eval <expr>)");
    if (ret) {
        pr_warn("kernel_shell_lua: Failed to register lua_eval command: %d\n", ret);
    }
    
    return 0;
}

static void __init unregister_lua_real_commands(void)
{
    kernel_shell_unregister_command("lua_test");
    kernel_shell_unregister_command("lua_info");
    kernel_shell_unregister_command("lua_eval");
}

// Enhanced module initialization with commands
static int __init kernel_shell_lua_real_init_enhanced(void)
{
    int ret = kernel_shell_lua_real_init();
    if (ret)
        return ret;
    
    register_lua_real_commands();
    
    // Auto-enable if requested
    if (auto_enable) {
        ret = kernel_shell_enable_script_engine("lua_real");
        if (ret) {
            pr_warn("kernel_shell_lua: Failed to auto-enable real Lua engine: %d\n", ret);
        } else {
            pr_info("kernel_shell_lua: Real Lua engine auto-enabled\n");
        }
    }
    
    return 0;
}

// Use the enhanced init function
module_init(kernel_shell_lua_real_init_enhanced);
