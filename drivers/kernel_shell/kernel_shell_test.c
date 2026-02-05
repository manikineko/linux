// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel Shell Test Module
 *
 * Copyright (C) 2024 Linux Kernel Developers
 *
 * This module tests the kernel shell functionality by triggering
 * a controlled panic to activate the shell.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel_shell.h>
#include <asm/io.h>  // For inb() function

MODULE_AUTHOR("Linux Kernel Developers");
MODULE_DESCRIPTION("Kernel Shell Test Module");
MODULE_LICENSE("GPL");

// Test command for the kernel shell
static void shell_cmd_test_input(const char *args)
{
    pr_info("kernel_shell_test: Input test command received!\n");
    pr_info("kernel_shell_test: Arguments: '%s'\n", args ? args : "(none)");
    pr_info("kernel_shell_test: If you can see this, input handling is working!\n");
}

static void shell_cmd_test_kbd(const char *args)
{
    int i, c;
    
    pr_info("kernel_shell_test: Testing direct keyboard input for 10 seconds...\n");
    pr_info("kernel_shell_test: Type some keys - they should appear here:\n");
    
    for (i = 0; i < 100; i++) { // 10 seconds (100 * 100ms)
        c = -1;
        
#ifdef CONFIG_X86
        // Try direct keyboard access
        if (inb(0x64) & 0x01) {
            c = inb(0x60);
            pr_info("kernel_shell_test: Raw scancode: 0x%02x\n", c);
        }
#endif
        
        if (c >= 0) {
            mdelay(10); // Small delay between key reads
        } else {
            mdelay(100); // Wait longer if no key
        }
    }
    
    pr_info("kernel_shell_test: Keyboard test completed.\n");
}

static void shell_cmd_test_panic(const char *args)
{
    pr_info("kernel_shell_test: Triggering test panic...\n");
    pr_info("kernel_shell_test: This should activate the kernel shell.\n");
    
    // Small delay to allow the message to be printed
    mdelay(100);
    
    // Trigger a panic to test the shell
    panic("kernel_shell_test: Test panic for shell input verification");
}

// Module initialization
static int __init kernel_shell_test_init(void)
{
    int ret;
    
    pr_info("kernel_shell_test: Loading Kernel Shell Test Module\n");
    
    // Register test commands
    ret = kernel_shell_register_command("test_input", shell_cmd_test_input, 
                                       "Test input handling in kernel shell");
    if (ret) {
        pr_warn("kernel_shell_test: Failed to register test_input command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("test_kbd", shell_cmd_test_kbd, 
                                       "Test direct keyboard input access");
    if (ret) {
        pr_warn("kernel_shell_test: Failed to register test_kbd command: %d\n", ret);
    }
    
    ret = kernel_shell_register_command("test_panic", shell_cmd_test_panic, 
                                       "Trigger a test panic to activate kernel shell");
    if (ret) {
        pr_warn("kernel_shell_test: Failed to register test_panic command: %d\n", ret);
    }
    
    pr_info("kernel_shell_test: Test commands registered\n");
    pr_info("kernel_shell_test: Use 'test_panic' command to trigger a panic and test the shell\n");
    pr_info("kernel_shell_test: Use 'test_input' command to test input handling\n");
    
    return 0;
}

// Module cleanup
static void __exit kernel_shell_test_exit(void)
{
    pr_info("kernel_shell_test: Unloading Kernel Shell Test Module\n");
    
    // Unregister test commands
    kernel_shell_unregister_command("test_input");
    kernel_shell_unregister_command("test_kbd");
    kernel_shell_unregister_command("test_panic");
    
    pr_info("kernel_shell_test: Test module unloaded\n");
}

module_init(kernel_shell_test_init);
module_exit(kernel_shell_test_exit);

// Module parameters
static bool auto_test = false;
module_param(auto_test, bool, 0644);
MODULE_PARM_DESC(auto_test, "Automatically trigger a test panic on module load");
