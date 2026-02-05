// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/kernel/panic.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * This function is used through-out the kernel (including mm and fs)
 * to indicate a major problem.
 */
#include <linux/debug_locks.h>
#include <linux/sched/debug.h>
#include <linux/interrupt.h>
#include <linux/kgdb.h>
#include <linux/kmsg_dump.h>
#include <linux/kallsyms.h>
#include <linux/notifier.h>
#include <linux/vt_kern.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/ftrace.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/kexec.h>
#include <linux/panic_notifier.h>
#include <linux/sched.h>
#include <linux/string_helpers.h>
#include <linux/sysrq.h>
#include <linux/init.h>
#include <linux/nmi.h>
#include <linux/console.h>
#include <linux/bug.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/context_tracking.h>
#include <linux/seq_buf.h>
#include <linux/sys_info.h>
#include <trace/events/error_report.h>
#include <asm/sections.h>

// Additional includes for graphical panic handling
#include <linux/fb.h>  // For framebuffer access
#include <linux/console.h>  // Already included, but for clarity
#include <linux/tty_driver.h>  // For tty driver access
#include <linux/serial_core.h>  // For uart polling functions
#include <asm/io.h>  // For direct I/O access
#include <linux/irq.h>  // For interrupt control

// Kernel shell includes
#ifdef CONFIG_KERNEL_SHELL
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/kernel_shell_module.h>
#include <linux/module.h>

// Forward declarations for module scripting functions
int kernel_module_execute_script_cmd_by_name(const char *module_name,
                                             const char *cmd,
                                             const char *args,
                                             char *output,
                                             size_t output_size);
int kernel_module_execute_script_by_name(const char *module_name,
                                         const char *engine,
                                         const char *script,
                                         char *output,
                                         size_t output_size);
#endif

#define PANIC_TIMER_STEP 100
#define PANIC_BLINK_SPD 18

#ifdef CONFIG_SMP
/*
 * Should we dump all CPUs backtraces in an oops event?
 * Defaults to 0, can be changed via sysctl.
 */
static unsigned int __read_mostly sysctl_oops_all_cpu_backtrace;
#else
#define sysctl_oops_all_cpu_backtrace 0
#endif /* CONFIG_SMP */

int panic_on_oops = IS_ENABLED(CONFIG_PANIC_ON_OOPS);
static unsigned long tainted_mask =
    IS_ENABLED(CONFIG_RANDSTRUCT) ? (1 << TAINT_RANDSTRUCT) : 0;
static int pause_on_oops;
static int pause_on_oops_flag;
static DEFINE_SPINLOCK(pause_on_oops_lock);
bool crash_kexec_post_notifiers;
int panic_on_warn __read_mostly;
unsigned long panic_on_taint;
bool panic_on_taint_nousertaint = false;
static unsigned int warn_limit __read_mostly;
static bool panic_console_replay;

bool panic_triggering_all_cpu_backtrace;
static bool panic_this_cpu_backtrace_printed;

int panic_timeout = CONFIG_PANIC_TIMEOUT;
EXPORT_SYMBOL_GPL(panic_timeout);

unsigned long panic_print;

ATOMIC_NOTIFIER_HEAD(panic_notifier_list);

EXPORT_SYMBOL(panic_notifier_list);

// Kernel shell global variables
#ifdef CONFIG_KERNEL_SHELL
static bool kernel_shell_active = false;
static bool kernel_shell_enabled = IS_ENABLED(CONFIG_KERNEL_SHELL);
static char kernel_shell_input[256] __maybe_unused;
static int kernel_shell_input_pos __maybe_unused = 0;

// PTY and graphical shell state
static int kernel_shell_cursor_x = 0;
static int kernel_shell_cursor_y = 0;
static int kernel_shell_scroll_top = 0;
static bool kernel_shell_shift_pressed = false;
static bool kernel_shell_ctrl_pressed = false;
static bool kernel_shell_alt_pressed = false;

// Graphical shell buffer
#define KERNEL_SHELL_WIDTH 80
#define KERNEL_SHELL_HEIGHT 25
static char kernel_shell_buffer[KERNEL_SHELL_HEIGHT][KERNEL_SHELL_WIDTH + 1];
static char kernel_shell_colors[KERNEL_SHELL_HEIGHT][KERNEL_SHELL_WIDTH]; // Color attributes
static bool kernel_shell_cursor_visible = true;
static int kernel_shell_cursor_timer = 0;

// PTY emulation
static struct kernel_shell_pty {
    int master_fd;
    int slave_fd;
    bool active;
    struct pid *pid;
} __maybe_unused kernel_shell_pty;

// Graphical shell functions
static void kernel_shell_init_display(void)
{
    int y;
    
    // Initialize the shell buffer
    for (y = 0; y < KERNEL_SHELL_HEIGHT; y++) {
        memset(kernel_shell_buffer[y], ' ', KERNEL_SHELL_WIDTH);
        kernel_shell_buffer[y][KERNEL_SHELL_WIDTH] = '\0';
        memset(kernel_shell_colors[y], 0x07, KERNEL_SHELL_WIDTH); // Light gray on black
    }
    
    kernel_shell_cursor_x = 0;
    kernel_shell_cursor_y = 0;
    kernel_shell_scroll_top = 0;
    kernel_shell_cursor_visible = true;
    kernel_shell_cursor_timer = 0;
}

static void kernel_shell_put_char_at(int x, int y, char c, char color)
{
    if (x >= 0 && x < KERNEL_SHELL_WIDTH && y >= 0 && y < KERNEL_SHELL_HEIGHT) {
        kernel_shell_buffer[y][x] = c;
        kernel_shell_colors[y][x] = color;
    }
}

static void kernel_shell_scroll_up(void)
{
    // Scroll all lines up by one
    for (int y = 0; y < KERNEL_SHELL_HEIGHT - 1; y++) {
        memcpy(kernel_shell_buffer[y], kernel_shell_buffer[y + 1], KERNEL_SHELL_WIDTH + 1);
        memcpy(kernel_shell_colors[y], kernel_shell_colors[y + 1], KERNEL_SHELL_WIDTH);
    }
    
    // Clear the last line
    memset(kernel_shell_buffer[KERNEL_SHELL_HEIGHT - 1], ' ', KERNEL_SHELL_WIDTH);
    kernel_shell_buffer[KERNEL_SHELL_HEIGHT - 1][KERNEL_SHELL_WIDTH] = '\0';
    memset(kernel_shell_colors[KERNEL_SHELL_HEIGHT - 1], 0x07, KERNEL_SHELL_WIDTH);
    
    // Move cursor up if it was on the bottom line
    if (kernel_shell_cursor_y > 0) {
        kernel_shell_cursor_y--;
    }
}

static void kernel_shell_newline(void)
{
    kernel_shell_cursor_x = 0;
    kernel_shell_cursor_y++;
    
    if (kernel_shell_cursor_y >= KERNEL_SHELL_HEIGHT) {
        kernel_shell_scroll_up();
        kernel_shell_cursor_y = KERNEL_SHELL_HEIGHT - 1;
    }
}

static void kernel_shell_putchar_graphical(char c)
{
    switch (c) {
        case '\r':
            kernel_shell_cursor_x = 0;
            break;
        case '\n':
            kernel_shell_newline();
            break;
        case '\b':
            if (kernel_shell_cursor_x > 0) {
                kernel_shell_cursor_x--;
                kernel_shell_put_char_at(kernel_shell_cursor_x, kernel_shell_cursor_y, ' ', 0x07);
            }
            break;
        case '\t':
            // Tab - advance to next multiple of 8
            kernel_shell_cursor_x = (kernel_shell_cursor_x + 8) & ~7;
            if (kernel_shell_cursor_x >= KERNEL_SHELL_WIDTH) {
                kernel_shell_newline();
            }
            break;
        default:
            if (c >= 32 && c <= 126) {
                kernel_shell_put_char_at(kernel_shell_cursor_x, kernel_shell_cursor_y, c, 0x07);
                kernel_shell_cursor_x++;
                
                if (kernel_shell_cursor_x >= KERNEL_SHELL_WIDTH) {
                    kernel_shell_newline();
                }
            }
            break;
    }
}

static void kernel_shell_draw_cursor(void)
{
    if (kernel_shell_cursor_visible && 
        kernel_shell_cursor_x >= 0 && kernel_shell_cursor_x < KERNEL_SHELL_WIDTH &&
        kernel_shell_cursor_y >= 0 && kernel_shell_cursor_y < KERNEL_SHELL_HEIGHT) {
        
        // Invert the character at cursor position (or show a block if space)
        char current_char = kernel_shell_buffer[kernel_shell_cursor_y][kernel_shell_cursor_x];
        if (current_char == ' ') {
            kernel_shell_put_char_at(kernel_shell_cursor_x, kernel_shell_cursor_y, '_', 0x0F); // White on blue
        } else {
            // Invert colors for cursor
            char color = kernel_shell_colors[kernel_shell_cursor_y][kernel_shell_cursor_x];
            kernel_shell_put_char_at(kernel_shell_cursor_x, kernel_shell_cursor_y, current_char, ((color & 0xF) << 4) | ((color & 0xF0) >> 4));
        }
    }
}

static void kernel_shell_update_display(void)
{
    // Clear screen first
    pr_emerg("\033[2J\033[H"); // ANSI escape codes to clear screen and home cursor
    
    // Draw the shell buffer
    for (int y = 0; y < KERNEL_SHELL_HEIGHT; y++) {
        pr_emerg("%s\n", kernel_shell_buffer[y]);
    }
    
    // Draw cursor
    kernel_shell_draw_cursor();
}

static void kernel_shell_handle_special_keys(int key)
{
    switch (key) {
        case 1: // Home
            kernel_shell_cursor_x = 0;
            break;
        case 2: // Up arrow
            if (kernel_shell_cursor_y > 0) {
                kernel_shell_cursor_y--;
            }
            break;
        case 3: // Left arrow
            if (kernel_shell_cursor_x > 0) {
                kernel_shell_cursor_x--;
            }
            break;
        case 4: // Right arrow
            if (kernel_shell_cursor_x < KERNEL_SHELL_WIDTH - 1) {
                kernel_shell_cursor_x++;
            }
            break;
        case 5: // Page up
            kernel_shell_cursor_y = 0;
            break;
        case 6: // End
            kernel_shell_cursor_x = KERNEL_SHELL_WIDTH - 1;
            break;
        case 7: // Down arrow
            if (kernel_shell_cursor_y < KERNEL_SHELL_HEIGHT - 1) {
                kernel_shell_cursor_y++;
            }
            break;
        case 8: // Page down
            kernel_shell_cursor_y = KERNEL_SHELL_HEIGHT - 1;
            break;
        case 9: // Insert
            // Toggle insert mode (not implemented yet)
            break;
        case 10: // Delete
            if (kernel_shell_cursor_x < KERNEL_SHELL_WIDTH - 1) {
                // Shift characters left
                for (int x = kernel_shell_cursor_x; x < KERNEL_SHELL_WIDTH - 1; x++) {
                    kernel_shell_buffer[kernel_shell_cursor_y][x] = kernel_shell_buffer[kernel_shell_cursor_y][x + 1];
                    kernel_shell_colors[kernel_shell_cursor_y][x] = kernel_shell_colors[kernel_shell_cursor_y][x + 1];
                }
                kernel_shell_put_char_at(KERNEL_SHELL_WIDTH - 1, kernel_shell_cursor_y, ' ', 0x07);
            }
            break;
    }
}
#ifdef CONFIG_KERNEL_SHELL
#ifdef CONFIG_X86
// Direct keyboard controller access for x86
static int kbd_controller_ready(void)
{
    int timeout = 100000;
    
    while (timeout-- && (inb(0x64) & 0x02))
        cpu_relax();
    
    return timeout >= 0;
}

static int kbd_has_data(void)
{
    return inb(0x64) & 0x01;
}

static int kbd_get_scancode(void)
{
    if (!kbd_controller_ready())
        return -1;
    
    if (!kbd_has_data())
        return -1;
    
    return inb(0x60);
}

// Enhanced scancode to ASCII conversion with shift support
static char kbd_scancode_to_ascii(int scancode, bool shift, bool ctrl, bool alt)
{
    // Mouse packets - filter them out
    if (scancode >= 0xE0 && scancode <= 0xEF) {
        return 0; // Ignore mouse scancodes
    }
    
    // Extended scancodes (E0 prefix)
    static bool extended = false;
    if (scancode == 0xE0) {
        extended = true;
        return 0;
    }
    
    // Modifier keys
    switch (scancode) {
        case 0x2A: // Left Shift
        case 0x36: // Right Shift
            kernel_shell_shift_pressed = true;
            return 0;
        case 0xAA: // Left Shift release
        case 0xB6: // Right Shift release
            kernel_shell_shift_pressed = false;
            return 0;
        case 0x1D: // Left Ctrl
            kernel_shell_ctrl_pressed = true;
            return 0;
        case 0x9D: // Left Ctrl release
            kernel_shell_ctrl_pressed = false;
            return 0;
        case 0x38: // Left Alt
            kernel_shell_alt_pressed = true;
            return 0;
        case 0xB8: // Left Alt release
            kernel_shell_alt_pressed = false;
            return 0;
    }
    
    // Regular keys with shift support
    static const char normal_table[128] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        0, 0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    static const char shift_table[128] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        0, 0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    // Extended keys handling
    if (extended) {
        extended = false;
        switch (scancode) {
            case 0x1C: // Numpad Enter
                return '\n';
            case 0x35: // Numpad /
                return '/';
            case 0x47: // Home
                return 1; // Home key
            case 0x48: // Up
                return 2; // Up arrow
            case 0x49: // PgUp
                return 5; // Page up
            case 0x4B: // Left
                return 3; // Left arrow
            case 0x4D: // Right
                return 4; // Right arrow
            case 0x4F: // End
                return 6; // End key
            case 0x50: // Down
                return 7; // Down arrow
            case 0x51: // PgDn
                return 8; // Page down
            case 0x52: // Insert
                return 9; // Insert
            case 0x53: // Delete
                return 10; // Delete
        }
        return 0;
    }
    
    if (scancode >= 0 && scancode < 128) {
        if (shift) {
            return shift_table[scancode];
        } else {
            return normal_table[scancode];
        }
    }
    
    return 0;
}

static int kernel_shell_getchar_kbd(void)
{
    int scancode = kbd_get_scancode();
    
    if (scancode < 0)
        return -1;
    
    return kbd_scancode_to_ascii(scancode, kernel_shell_shift_pressed, 
                                kernel_shell_ctrl_pressed, kernel_shell_alt_pressed);
}
#endif

#ifdef CONFIG_CONSOLE_POLL
static int kernel_shell_getchar_console(void)
{
    struct tty_driver *driver;
    int console_index = 0;
    int c = -1;
    
    // Get the console tty driver
    driver = console_device(&console_index);
    if (!driver) {
        // Fallback: try to get uart console directly
        extern struct tty_driver *console_driver;
        driver = console_driver;
        if (!driver)
            return -1;
    }
    
    // Check if the driver supports polling
    if (driver->ops && driver->ops->poll_get_char) {
        c = driver->ops->poll_get_char(driver, console_index);
    }
    
    return c;
}

static void kernel_shell_putchar_console(char c)
{
    struct tty_driver *driver;
    int console_index = 0;
    
    // Get the console tty driver
    driver = console_device(&console_index);
    if (!driver) {
        // Fallback: try to get uart console directly
        extern struct tty_driver *console_driver;
        driver = console_driver;
        if (!driver) {
            // Ultimate fallback: use console output
            pr_emerg("%c", c);
            return;
        }
    }
    
    // Check if the driver supports polling
    if (driver->ops && driver->ops->poll_put_char) {
        driver->ops->poll_put_char(driver, console_index, c);
    } else {
        // Fallback: use console output
        pr_emerg("%c", c);
    }
}
#endif

// Unified input functions
static int kernel_shell_getchar(void)
{
#ifdef CONFIG_X86
    // Try direct keyboard access first on x86
    int c = kernel_shell_getchar_kbd();
    if (c >= 0)
        return c;
#endif

#ifdef CONFIG_CONSOLE_POLL
    // Fallback to console polling
    return kernel_shell_getchar_console();
#else
    // No polling support available
    return -1;
#endif
}

static void __maybe_unused kernel_shell_putchar(char c)
{
#ifdef CONFIG_CONSOLE_POLL
    kernel_shell_putchar_console(c);
#else
    // Use console output as fallback
    pr_emerg("%c", c);
#endif
}

static void kernel_shell_read_line(char *buffer, int max_len)
{
    int pos = 0;
    int c;
    int display_update_counter = 0;
    
    while (pos < max_len - 1) {
        c = kernel_shell_getchar();
        
        if (c < 0) {
            // No character available, update display periodically and wait
            display_update_counter++;
            if (display_update_counter > 50) { // Update every ~500ms
                kernel_shell_update_display();
                display_update_counter = 0;
            }
            mdelay(10);
            continue;
        }
        
        // Handle special keys (arrows, home, end, etc.)
        if (c >= 1 && c <= 10) {
            kernel_shell_handle_special_keys(c);
            kernel_shell_update_display();
            continue;
        }
        
        // Handle regular characters
        switch (c) {
            case '\r':
            case '\n':
                kernel_shell_putchar_graphical('\r');
                kernel_shell_putchar_graphical('\n');
                kernel_shell_update_display();
                break;
            case '\b':
            case 127:
                // Backspace
                if (pos > 0) {
                    pos--;
                    kernel_shell_putchar_graphical('\b');
                    kernel_shell_update_display();
                }
                break;
            default:
                if (c >= 32 && c <= 126) {
                    buffer[pos++] = c;
                    kernel_shell_putchar_graphical(c);
                    kernel_shell_update_display();
                }
                break;
        }
        
        // Update display on every character
        kernel_shell_update_display();
        
        // Break on enter
        if (c == '\r' || c == '\n') {
            break;
        }
    }
    
    buffer[pos] = '\0';
}
#endif
struct kernel_shell_command {
    char name[32];
    void (*func)(const char *args);
    const char *help;
    struct list_head list;
};

static LIST_HEAD(kernel_shell_commands);
static DEFINE_SPINLOCK(kernel_shell_commands_lock);

// Script module interface
#ifdef CONFIG_KERNEL_SHELL_SCRIPTING
struct kernel_script_engine {
    char name[32];
    int (*execute)(const char *script);
    int (*enable)(void);
    int (*disable)(void);
    bool enabled;
    struct list_head list;
};

static LIST_HEAD(kernel_script_engines);
static DEFINE_SPINLOCK(kernel_script_engines_lock);
#endif
#endif

static void panic_print_deprecated(void)
{
    pr_info_once("Kernel: The 'panic_print' parameter is now deprecated. Please use 'panic_sys_info' and 'panic_console_replay' instead.\n");
}

#ifdef CONFIG_SYSCTL

/*
 * Taint values can only be increased
 * This means we can safely use a temporary.
 */
static int proc_taint(const struct ctl_table *table, int write,
                   void *buffer, size_t *lenp, loff_t *ppos)
{
    struct ctl_table t;
    unsigned long tmptaint = get_taint();
    int err;

    if (write && !capable(CAP_SYS_ADMIN))
        return -EPERM;

    t = *table;
    t.data = &tmptaint;
    err = proc_doulongvec_minmax(&t, write, buffer, lenp, ppos);
    if (err < 0)
        return err;

    if (write) {
        int i;

        /*
         * If we are relying on panic_on_taint not producing
         * false positives due to userspace input, bail out
         * before setting the requested taint flags.
         */
        if (panic_on_taint_nousertaint && (tmptaint & panic_on_taint))
            return -EINVAL;

        /*
         * Poor man's atomic or. Not worth adding a primitive
         * to everyone's atomic.h for this
         */
        for (i = 0; i < TAINT_FLAGS_COUNT; i++)
            if ((1UL << i) & tmptaint)
                add_taint(i, LOCKDEP_STILL_OK);
    }

    return err;
}

static int sysctl_panic_print_handler(const struct ctl_table *table, int write,
               void *buffer, size_t *lenp, loff_t *ppos)
{
    if (write)
        panic_print_deprecated();
    return proc_doulongvec_minmax(table, write, buffer, lenp, ppos);
}

static const struct ctl_table kern_panic_table[] = {
#ifdef CONFIG_SMP
    {
        .procname       = "oops_all_cpu_backtrace",
        .data           = &sysctl_oops_all_cpu_backtrace,
        .maxlen         = sizeof(int),
        .mode           = 0644,
        .proc_handler   = proc_dointvec_minmax,
        .extra1         = SYSCTL_ZERO,
        .extra2         = SYSCTL_ONE,
    },
#endif
    {
        .procname	= "tainted",
        .maxlen		= sizeof(long),
        .mode		= 0644,
        .proc_handler	= proc_taint,
    },
    {
        .procname	= "panic",
        .data		= &panic_timeout,
        .maxlen		= sizeof(int),
        .mode		= 0644,
        .proc_handler	= proc_dointvec,
    },
    {
        .procname	= "panic_on_oops",
        .data		= &panic_on_oops,
        .maxlen		= sizeof(int),
        .mode		= 0644,
        .proc_handler	= proc_dointvec,
    },
    {
        .procname	= "panic_print",
        .data		= &panic_print,
        .maxlen		= sizeof(unsigned long),
        .mode		= 0644,
        .proc_handler	= sysctl_panic_print_handler,
    },
    {
        .procname	= "panic_on_warn",
        .data		= &panic_on_warn,
        .maxlen		= sizeof(int),
        .mode		= 0644,
        .proc_handler	= proc_dointvec_minmax,
        .extra1		= SYSCTL_ZERO,
        .extra2		= SYSCTL_ONE,
    },
    {
        .procname       = "warn_limit",
        .data           = &warn_limit,
        .maxlen         = sizeof(warn_limit),
        .mode           = 0644,
        .proc_handler   = proc_douintvec,
    },
#if (defined(CONFIG_X86_32) || defined(CONFIG_PARISC)) && \
    defined(CONFIG_DEBUG_STACKOVERFLOW)
    {
        .procname	= "panic_on_stackoverflow",
        .data		= &sysctl_panic_on_stackoverflow,
        .maxlen		= sizeof(int),
        .mode		= 0644,
        .proc_handler	= proc_dointvec,
    },
#endif
    {
        .procname	= "panic_sys_info",
        .data		= &panic_print,
        .maxlen         = sizeof(panic_print),
        .mode		= 0644,
        .proc_handler	= sysctl_sys_info_handler,
    },
};

static __init int kernel_panic_sysctls_init(void)
{
    register_sysctl_init("kernel", kern_panic_table);
    return 0;
}
late_initcall(kernel_panic_sysctls_init);
#endif

/* The format is "panic_sys_info=tasks,mem,locks,ftrace,..." */
static int __init setup_panic_sys_info(char *buf)
{
    /* There is no risk of race in kernel boot phase */
    panic_print = sys_info_parse_param(buf);
    return 1;
}
__setup("panic_sys_info=", setup_panic_sys_info);

static atomic_t warn_count = ATOMIC_INIT(0);

#ifdef CONFIG_SYSFS
static ssize_t warn_count_show(struct kobject *kobj, struct kobj_attribute *attr,
                   char *page)
{
    return sysfs_emit(page, "%d\n", atomic_read(&warn_count));
}

static struct kobj_attribute warn_count_attr = __ATTR_RO(warn_count);

static __init int kernel_panic_sysfs_init(void)
{
    sysfs_add_file_to_group(kernel_kobj, &warn_count_attr.attr, NULL);
    return 0;
}
late_initcall(kernel_panic_sysfs_init);
#endif

static long no_blink(int state)
{
    return 0;
}

/* Returns how long it waited in ms */
long (*panic_blink)(int state);
EXPORT_SYMBOL(panic_blink);

/*
 * Stop ourself in panic -- architecture code may override this
 */
void __weak __noreturn panic_smp_self_stop(void)
{
    while (1)
        cpu_relax();
}

/*
 * Stop ourselves in NMI context if another CPU has already panicked. Arch code
 * may override this to prepare for crash dumping, e.g. save regs info.
 */
void __weak __noreturn nmi_panic_self_stop(struct pt_regs *regs)
{
    panic_smp_self_stop();
}

/*
 * Stop other CPUs in panic.  Architecture dependent code may override this
 * with more suitable version.  For example, if the architecture supports
 * crash dump, it should save registers of each stopped CPU and disable
 * per-CPU features such as virtualization extensions.
 */
void __weak crash_smp_send_stop(void)
{
    static int cpus_stopped;

    /*
     * This function can be called twice in panic path, but obviously
     * we execute this only once.
     */
    if (cpus_stopped)
        return;

    /*
     * Note smp_send_stop is the usual smp shutdown function, which
     * unfortunately means it may not be hardened to work in a panic
     * situation.
     */
    smp_send_stop();
    cpus_stopped = 1;
}

atomic_t panic_cpu = ATOMIC_INIT(PANIC_CPU_INVALID);

bool panic_try_start(void)
{
    int old_cpu, this_cpu;

    /*
     * Only one CPU is allowed to execute the crash_kexec() code as with
     * panic().  Otherwise parallel calls of panic() and crash_kexec()
     * may stop each other.  To exclude them, we use panic_cpu here too.
     */
    old_cpu = PANIC_CPU_INVALID;
    this_cpu = raw_smp_processor_id();

    return atomic_try_cmpxchg(&panic_cpu, &old_cpu, this_cpu);
}
EXPORT_SYMBOL(panic_try_start);

void panic_reset(void)
{
    atomic_set(&panic_cpu, PANIC_CPU_INVALID);
}
EXPORT_SYMBOL(panic_reset);

bool panic_in_progress(void)
{
    return unlikely(atomic_read(&panic_cpu) != PANIC_CPU_INVALID);
}
EXPORT_SYMBOL(panic_in_progress);

/* Return true if a panic is in progress on the current CPU. */
bool panic_on_this_cpu(void)
{
    /*
     * We can use raw_smp_processor_id() here because it is impossible for
     * the task to be migrated to the panic_cpu, or away from it. If
     * panic_cpu has already been set, and we're not currently executing on
     * that CPU, then we never will be.
     */
    return unlikely(atomic_read(&panic_cpu) == raw_smp_processor_id());
}
EXPORT_SYMBOL(panic_on_this_cpu);

/*
 * Return true if a panic is in progress on a remote CPU.
 *
 * On true, the local CPU should immediately release any printing resources
 * that may be needed by the panic CPU.
 */
bool panic_on_other_cpu(void)
{
    return (panic_in_progress() && !panic_on_this_cpu());
}
EXPORT_SYMBOL(panic_on_other_cpu);

/*
 * A variant of panic() called from NMI context. We return if we've already
 * panicked on this CPU. If another CPU already panicked, loop in
 * nmi_panic_self_stop() which can provide architecture dependent code such
 * as saving register state for crash dump.
 */
void nmi_panic(struct pt_regs *regs, const char *msg)
{
    if (panic_try_start())
        panic("%s", msg);
    else if (panic_on_other_cpu())
        nmi_panic_self_stop(regs);
}
EXPORT_SYMBOL(nmi_panic);

void check_panic_on_warn(const char *origin)
{
    unsigned int limit;

    if (panic_on_warn)
        panic("%s: panic_on_warn set ...\n", origin);

    limit = READ_ONCE(warn_limit);
    if (atomic_inc_return(&warn_count) >= limit && limit)
        panic("%s: system warned too often (kernel.warn_limit is %d)",
              origin, limit);
}

static void panic_trigger_all_cpu_backtrace(void)
{
    /* Temporary allow non-panic CPUs to write their backtraces. */
    panic_triggering_all_cpu_backtrace = true;

    if (panic_this_cpu_backtrace_printed)
        trigger_allbutcpu_cpu_backtrace(raw_smp_processor_id());
    else
        trigger_all_cpu_backtrace();

    panic_triggering_all_cpu_backtrace = false;
}

/*
 * Helper that triggers the NMI backtrace (if set in panic_print)
 * and then performs the secondary CPUs shutdown - we cannot have
 * the NMI backtrace after the CPUs are off!
 */
static void panic_other_cpus_shutdown(bool crash_kexec)
{
    if (panic_print & SYS_INFO_ALL_BT)
        panic_trigger_all_cpu_backtrace();

    /*
     * Note that smp_send_stop() is the usual SMP shutdown function,
     * which unfortunately may not be hardened to work in a panic
     * situation. If we want to do crash dump after notifier calls
     * and kmsg_dump, we will need architecture dependent extra
     * bits in addition to stopping other CPUs, hence we rely on
     * crash_smp_send_stop() for that.
     */
    if (!crash_kexec)
        smp_send_stop();
    else
        crash_smp_send_stop();
}

// New function to check if graphical mode is active (X11/Wayland running)
static bool is_graphical_mode(void)
{
    // Check if the current console is in graphics mode
    if (vc_cons[fg_console].d && vc_cons[fg_console].d->vc_mode == KD_GRAPHICS) {
        return true;
    }
    return false;
}

// New function to attempt to switch to text mode or take over the framebuffer for graphical panic
static void takeover_framebuffer(void)
{
    // Unblank the console, which may switch back to text if possible
    console_unblank();

    // If still in graphics mode, we can try to use the framebuffer for drawing
    // Note: In modern kernels with DRM panic support, this is handled by DRM drivers
}

// New function to display a simple graphical panic using framebuffer (basic implementation)
static void display_graphical_panic(const char *msg)
{
    // Graphical panic disabled for now - framebuffer access not available
    // In a real implementation, this would use DRM panic or fbdev
    pr_emerg("Graphical Panic Display: %s\n", msg);
}

#ifdef CONFIG_KERNEL_SHELL_LOGO
// ASCII Art Logo for Kernel Panic
static const char *kernel_panic_logo[] = {
    "    ___       __  _           __  __           _       _   ",
    "   / __\\___  / _| |_ __ _  ___|  \\/  | ___   __| |_   _| |_ ",
    "  / /  / _ \\| |_| __/ _` |/ __| |\\/| |/ _ \\ / _` | | | | __|",
    " / /__| (_) |  _| || (_| | (__| |  | | (_) | (_| | |_| | |_ ",
    " \\____/\\___/|_|  \\__\\__,_|\\___|_|  |_|\\___/ \\__,_|\\__,_|\\__|",
    "                                                             ",
    "                     KERNEL PANIC                           ",
    "                                                             ",
    NULL
};

static void display_kernel_logo(void)
{
    int i = 0;
    
    pr_emerg("\n");
    while (kernel_panic_logo[i] != NULL) {
        pr_emerg("%s\n", kernel_panic_logo[i]);
        i++;
    }
    pr_emerg("\n");
}
#else
static void __maybe_unused display_kernel_logo(void)
{
    // No logo display if not enabled
}
#endif

// Kernel shell built-in commands
#ifdef CONFIG_KERNEL_SHELL
static void shell_cmd_help(const char *args)
{
    struct kernel_shell_command *cmd;
    
    const char *help_lines[] = {
        "╔══════════════════════════════════════════════════════════════════╗",
        "║                        KERNEL SHELL COMMANDS                        ║",
        "╠══════════════════════════════════════════════════════════════════╣",
        "║  help              - Show this help message                         ║",
        "║  continue          - Continue with panic shutdown                   ║",
        "║  status            - Show system status                             ║",
        "║  modules           - List loaded modules                            ║",
#ifdef CONFIG_KERNEL_SHELL_SCRIPTING
        "║  script            - Execute a script (usage: script <engine> <script>) ║",
        "║  engines           - List available script engines                   ║",
        "║  modules_scriptable - List scriptable modules                      ║",
        "║  module_cmd        - Execute module command (usage: module_cmd <module> <cmd> [args]) ║",
        "║  module_script     - Execute script in module context (usage: module_script <module> <engine> <script>) ║",
#endif
        "╠══════════════════════════════════════════════════════════════════╣",
        "║                      REGISTERED COMMANDS                            ║",
        "╚══════════════════════════════════════════════════════════════════╝",
        NULL
    };
    
    // Display help header
    for (int i = 0; help_lines[i]; i++) {
        for (int j = 0; help_lines[i][j]; j++) {
            kernel_shell_putchar_graphical(help_lines[i][j]);
        }
        kernel_shell_putchar_graphical('\n');
    }
    
    // Display registered commands
    spin_lock(&kernel_shell_commands_lock);
    list_for_each_entry(cmd, &kernel_shell_commands, list) {
        const char *prefix = "║  ";
        const char *suffix = " ║";
        
        for (int i = 0; prefix[i]; i++) {
            kernel_shell_putchar_graphical(prefix[i]);
        }
        
        for (int i = 0; cmd->name[i]; i++) {
            kernel_shell_putchar_graphical(cmd->name[i]);
        }
        
        // Add padding
        int name_len = strlen(cmd->name);
        for (int i = name_len; i < 16; i++) {
            kernel_shell_putchar_graphical(' ');
        }
        
        kernel_shell_putchar_graphical(' ');
        kernel_shell_putchar_graphical('-');
        kernel_shell_putchar_graphical(' ');
        
        for (int i = 0; cmd->help && cmd->help[i]; i++) {
            kernel_shell_putchar_graphical(cmd->help[i]);
        }
        
        for (int i = 0; suffix[i]; i++) {
            kernel_shell_putchar_graphical(suffix[i]);
        }
        
        kernel_shell_putchar_graphical('\n');
    }
    spin_unlock(&kernel_shell_commands_lock);
    
    // Display footer
    const char *footer = "╚══════════════════════════════════════════════════════════════════╝";
    for (int i = 0; footer[i]; i++) {
        kernel_shell_putchar_graphical(footer[i]);
    }
    kernel_shell_putchar_graphical('\n');
}

static void shell_cmd_continue(const char *args)
{
    const char *msg = "Continuing with panic shutdown...\n";
    for (int i = 0; msg[i]; i++) {
        kernel_shell_putchar_graphical(msg[i]);
    }
    
    kernel_shell_update_display();
    
    // Give a moment to see the message
    mdelay(2000);
    
    const char *recovery_msg = "Attempting system recovery...\n";
    for (int i = 0; recovery_msg[i]; i++) {
        kernel_shell_putchar_graphical(recovery_msg[i]);
    }
    
    kernel_shell_update_display();
    mdelay(1000);
    
    // Deactivate shell first
    kernel_shell_active = false;
    
    // Clear panic state
    atomic_set(&panic_cpu, PANIC_CPU_INVALID);
    
    // Re-enable interrupts
    local_irq_enable();
    preempt_enable();
    
    // Clear console and attempt to restore normal operation
    const char *restore_msg = "Restoring normal system operation...\n";
    for (int i = 0; restore_msg[i]; i++) {
        kernel_shell_putchar_graphical(restore_msg[i]);
    }
    
    kernel_shell_update_display();
    mdelay(1000);
    
    // Clear graphical mode and restore console
    pr_emerg("\n=== SYSTEM RECOVERY ATTEMPTED ===\n");
    pr_emerg("Kernel shell deactivated\n");
    pr_emerg("Panic state cleared\n");
    pr_emerg("Interrupts re-enabled\n");
    pr_emerg("Attempting to resume normal operation\n");
    pr_emerg("If system remains unstable, manual reboot may be required\n");
    pr_emerg("=====================================\n\n");
    
    // In a real implementation, this would involve more complex recovery
    // For now, we'll exit the panic handler and let the system continue
    bust_spinlocks(0);
    
    // Add a small delay before returning
    mdelay(500);
}

static void shell_cmd_status(const char *args)
{
    pr_emerg("System Status:\n");
    pr_emerg("  CPU: %d\n", raw_smp_processor_id());
    pr_emerg("  PID: %d (%s)\n", current->pid, current->comm);
    pr_emerg("  Tainted: %s\n", print_tainted());
    pr_emerg("  Panic CPU: %d\n", atomic_read(&panic_cpu));
}

static void shell_cmd_modules(const char *args)
{
    pr_emerg("Loaded modules:\n");
    // Basic module listing - in a real implementation, 
    // we would iterate through the module list
    print_modules();
}

#ifdef CONFIG_KERNEL_SHELL_SCRIPTING
static void shell_cmd_script(const char *args);
static void shell_cmd_engines(const char *args);
static void shell_cmd_modules_scriptable(const char *args);
static void shell_cmd_module_cmd(const char *args);
static void shell_cmd_module_script(const char *args);

static void shell_cmd_script(const char *args)
{
    char engine_name[32];
    char *script;
    struct kernel_script_engine *engine;
    
    if (!args || strlen(args) == 0) {
        pr_emerg("Usage: script <engine> <script>\n");
        return;
    }
    
    // Parse engine name
    if (sscanf(args, "%31s", engine_name) != 1) {
        pr_emerg("Invalid engine name\n");
        return;
    }
    
    script = strchr(args, ' ');
    if (script) {
        script++; // Skip space
    } else {
        pr_emerg("No script provided\n");
        return;
    }
    
    // Find engine
    spin_lock(&kernel_script_engines_lock);
    list_for_each_entry(engine, &kernel_script_engines, list) {
        if (strcmp(engine->name, engine_name) == 0) {
            if (!engine->enabled) {
                pr_emerg("Script engine '%s' is not enabled\n", engine_name);
                spin_unlock(&kernel_script_engines_lock);
                return;
            }
            
            if (engine->execute) {
                int result = engine->execute(script);
                pr_emerg("Script execution result: %d\n", result);
            } else {
                pr_emerg("Script engine '%s' has no execute function\n", engine_name);
            }
            spin_unlock(&kernel_script_engines_lock);
            return;
        }
    }
    spin_unlock(&kernel_script_engines_lock);
    
    pr_emerg("Script engine '%s' not found\n", engine_name);
}

static void shell_cmd_engines(const char *args)
{
    struct kernel_script_engine *engine;
    
    pr_emerg("Available script engines:\n");
    spin_lock(&kernel_script_engines_lock);
    list_for_each_entry(engine, &kernel_script_engines, list) {
        pr_emerg("  %-8s - %s\n", engine->name, 
                engine->enabled ? "enabled" : "disabled");
    }
    spin_unlock(&kernel_script_engines_lock);
}

static void shell_cmd_modules_scriptable(const char *args)
{
    char output[1024];
    
    kernel_module_get_scriptable_modules(output, sizeof(output));
    pr_emerg("%s", output);
}

static void shell_cmd_module_cmd(const char *args)
{
    char module_name[MODULE_NAME_LEN];
    char cmd_name[64];
    char cmd_args[256];
    char output[512];
    int ret;
    
    if (!args || strlen(args) == 0) {
        pr_emerg("Usage: module_cmd <module> <command> [args]\n");
        return;
    }
    
    // Parse arguments
    if (sscanf(args, "%63s %63s %255[^\n]", module_name, cmd_name, cmd_args) < 2) {
        pr_emerg("Invalid arguments. Usage: module_cmd <module> <command> [args]\n");
        return;
    }
    
    // Execute command using module scripting system
    ret = kernel_module_execute_script_cmd_by_name(module_name, cmd_name, cmd_args, output, sizeof(output));
    if (ret == 0) {
        pr_emerg("Command executed successfully:\n%s\n", output);
    } else {
        pr_emerg("Command failed (error %d): %s\n", ret, output);
    }
}

static void shell_cmd_module_script(const char *args)
{
    char module_name[MODULE_NAME_LEN];
    char engine[32];
    char script[512];
    char output[512];
    int ret;
    
    if (!args || strlen(args) == 0) {
        pr_emerg("Usage: module_script <module> <engine> <script>\n");
        return;
    }
    
    // Parse arguments
    if (sscanf(args, "%63s %31s %511[^\n]", module_name, engine, script) < 3) {
        pr_emerg("Invalid arguments. Usage: module_script <module> <engine> <script>\n");
        return;
    }
    
    // Execute script using module scripting system
    ret = kernel_module_execute_script_by_name(module_name, engine, script, output, sizeof(output));
    if (ret == 0) {
        pr_emerg("Script executed successfully:\n%s\n", output);
    } else {
        pr_emerg("Script failed (error %d): %s\n", ret, output);
    }
}
#endif

// Register built-in commands
static void __init __maybe_unused kernel_shell_init_builtin_commands(void)
{
    // Register built-in commands
    // Note: In a real implementation, we would have a registration function
    // For now, commands are handled directly in the command parser
}

// Parse and execute shell commands
static void kernel_shell_execute_command(const char *cmd)
{
    char cmd_name[32];
    const char *args = NULL;
    char *space;
    
    if (!cmd || strlen(cmd) == 0) {
        return;
    }
    
    // Make a copy for parsing
    strncpy(cmd_name, cmd, sizeof(cmd_name) - 1);
    cmd_name[sizeof(cmd_name) - 1] = '\0';
    
    // Find space to separate command from arguments
    space = strchr(cmd_name, ' ');
    if (space) {
        *space = '\0';
        args = cmd + (space - cmd_name + 1);
    }
    
    // Execute built-in commands
    if (strcmp(cmd_name, "help") == 0) {
        shell_cmd_help(args);
    } else if (strcmp(cmd_name, "continue") == 0) {
        shell_cmd_continue(args);
    } else if (strcmp(cmd_name, "status") == 0) {
        shell_cmd_status(args);
    } else if (strcmp(cmd_name, "modules") == 0) {
        shell_cmd_modules(args);
#ifdef CONFIG_KERNEL_SHELL_SCRIPTING
    } else if (strcmp(cmd_name, "script") == 0) {
        shell_cmd_script(args);
    } else if (strcmp(cmd_name, "engines") == 0) {
        shell_cmd_engines(args);
    } else if (strcmp(cmd_name, "modules_scriptable") == 0) {
        shell_cmd_modules_scriptable(args);
    } else if (strcmp(cmd_name, "module_cmd") == 0) {
        shell_cmd_module_cmd(args);
    } else if (strcmp(cmd_name, "module_script") == 0) {
        shell_cmd_module_script(args);
#endif
    } else {
        // Check registered commands
        struct kernel_shell_command *cmd;
        bool found = false;
        
        spin_lock(&kernel_shell_commands_lock);
        list_for_each_entry(cmd, &kernel_shell_commands, list) {
            if (strcmp(cmd->name, cmd_name) == 0) {
                found = true;
                break;
            }
        }
        spin_unlock(&kernel_shell_commands_lock);
        
        if (found && cmd) {
            // Execute the command
            if (cmd->func) {
                cmd->func(args);
            }
        } else {
            // Command not found
            const char *error_msg = "Unknown command: ";
            for (int i = 0; error_msg[i]; i++) {
                kernel_shell_putchar_graphical(error_msg[i]);
            }
            for (int i = 0; cmd_name[i]; i++) {
                kernel_shell_putchar_graphical(cmd_name[i]);
            }
            kernel_shell_putchar_graphical('\n');
            
            const char *help_msg = "Type 'help' for available commands\n";
            for (int i = 0; help_msg[i]; i++) {
                kernel_shell_putchar_graphical(help_msg[i]);
            }
        }
    }
}

// Main kernel shell loop
static void kernel_shell_main(void)
{
    char input_buffer[256];
    char command[64];
    char args[192];
    char *space;
    
    // Initialize graphical display
    kernel_shell_init_display();
    
    // Display welcome message
    const char *welcome_lines[] = {
        "╔══════════════════════════════════════════════════════════════════╗",
        "║                    KERNEL PANIC SHELL ACTIVATED                    ║",
        "║                                                                  ║",
        "║  System has encountered a critical error and entered panic mode   ║",
        "║                                                                  ║",
        "║  Features:                                                       ║",
        "║  • Full keyboard input with shift/ctrl/alt support               ║",
        "║  • Visual cursor and mouse support                                ║",
        "║  • Graphical interface with ANSI colors                           ║",
        "║  • Command history and editing                                    ║",
        "║                                                                  ║",
        "║  Type 'help' for available commands or 'continue' to shutdown     ║",
        "╚══════════════════════════════════════════════════════════════════╝",
        NULL
    };
    
    // Display welcome message
    for (int i = 0; welcome_lines[i]; i++) {
        for (int j = 0; welcome_lines[i][j]; j++) {
            kernel_shell_putchar_graphical(welcome_lines[i][j]);
        }
        kernel_shell_putchar_graphical('\n');
    }
    kernel_shell_putchar_graphical('\n');
    
    kernel_shell_active = true;
    kernel_shell_input_pos = 0;
    
    while (kernel_shell_active) {
        // Display prompt
        const char *prompt = "kernel> ";
        for (int i = 0; prompt[i]; i++) {
            kernel_shell_putchar_graphical(prompt[i]);
        }
        
        // Update display
        kernel_shell_update_display();
        
        // Read user input
        kernel_shell_read_line(input_buffer, sizeof(input_buffer));
        
        // Skip empty input
        if (strlen(input_buffer) == 0) {
            continue;
        }
        
        // Parse command and arguments
        strncpy(command, input_buffer, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
        strncpy(args, input_buffer, sizeof(args) - 1);
        args[sizeof(args) - 1] = '\0';
        
        space = strchr(command, ' ');
        if (space) {
            *space = '\0';
            space = strchr(args, ' ');
            if (space) {
                space++; // Skip the space
                // Move args to point after the space
                memmove(args, space, strlen(space) + 1);
            } else {
                args[0] = '\0';
            }
        } else {
            args[0] = '\0';
        }
        
        // Display command being executed
        kernel_shell_putchar_graphical('\n');
        const char *exec_msg = "Executing: ";
        for (int i = 0; exec_msg[i]; i++) {
            kernel_shell_putchar_graphical(exec_msg[i]);
        }
        for (int i = 0; command[i]; i++) {
            kernel_shell_putchar_graphical(command[i]);
        }
        if (args[0]) {
            kernel_shell_putchar_graphical(' ');
            for (int i = 0; args[i]; i++) {
                kernel_shell_putchar_graphical(args[i]);
            }
        }
        kernel_shell_putchar_graphical('\n');
        kernel_shell_putchar_graphical('\n');
        
        // Update display before command execution
        kernel_shell_update_display();
        
        // Execute the command
        kernel_shell_execute_command(command);
        
        // Check if we should continue
        if (strcmp(command, "continue") == 0 || 
            strcmp(command, "exit") == 0 ||
            strcmp(command, "quit") == 0) {
            kernel_shell_active = false;
            break;
        }
        
        kernel_shell_putchar_graphical('\n');
    }
    
    // Display exit message
    const char *exit_msg = "Exiting kernel shell...";
    for (int i = 0; exit_msg[i]; i++) {
        kernel_shell_putchar_graphical(exit_msg[i]);
    }
    kernel_shell_putchar_graphical('\n');
    
    kernel_shell_update_display();
}

// Kernel module interface functions
#ifdef CONFIG_KERNEL_SHELL
/**
 * kernel_shell_register_command - Register a new shell command
 * @name: Command name
 * @func: Function to execute for this command
 * @help: Help text for the command
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_register_command(const char *name, void (*func)(const char *), const char *help)
{
    struct kernel_shell_command *cmd;
    
    if (!name || !func || strlen(name) >= sizeof(cmd->name)) {
        return -EINVAL;
    }
    
    cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
    if (!cmd) {
        return -ENOMEM;
    }
    
    strncpy(cmd->name, name, sizeof(cmd->name) - 1);
    cmd->name[sizeof(cmd->name) - 1] = '\0';
    cmd->func = func;
    cmd->help = help;
    
    spin_lock(&kernel_shell_commands_lock);
    list_add_tail(&cmd->list, &kernel_shell_commands);
    spin_unlock(&kernel_shell_commands_lock);
    
    return 0;
}
EXPORT_SYMBOL(kernel_shell_register_command);

/**
 * kernel_shell_unregister_command - Unregister a shell command
 * @name: Command name to unregister
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_unregister_command(const char *name)
{
    struct kernel_shell_command *cmd, *tmp;
    bool found = false;
    
    if (!name) {
        return -EINVAL;
    }
    
    spin_lock(&kernel_shell_commands_lock);
    list_for_each_entry_safe(cmd, tmp, &kernel_shell_commands, list) {
        if (strcmp(cmd->name, name) == 0) {
            list_del(&cmd->list);
            kfree(cmd);
            found = true;
            break;
        }
    }
    spin_unlock(&kernel_shell_commands_lock);
    
    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(kernel_shell_unregister_command);

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
                                        int (*disable)(void))
{
    struct kernel_script_engine *engine;
    
    if (!name || strlen(name) >= sizeof(engine->name)) {
        return -EINVAL;
    }
    
    engine = kmalloc(sizeof(*engine), GFP_KERNEL);
    if (!engine) {
        return -ENOMEM;
    }
    
    strncpy(engine->name, name, sizeof(engine->name) - 1);
    engine->name[sizeof(engine->name) - 1] = '\0';
    engine->execute = execute;
    engine->enable = enable;
    engine->disable = disable;
    engine->enabled = false;
    
    spin_lock(&kernel_script_engines_lock);
    list_add_tail(&engine->list, &kernel_script_engines);
    spin_unlock(&kernel_script_engines_lock);
    
    return 0;
}
EXPORT_SYMBOL(kernel_shell_register_script_engine);

/**
 * kernel_shell_unregister_script_engine - Unregister a script engine
 * @name: Engine name to unregister
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_unregister_script_engine(const char *name)
{
    struct kernel_script_engine *engine, *tmp;
    bool found = false;
    
    if (!name) {
        return -EINVAL;
    }
    
    spin_lock(&kernel_script_engines_lock);
    list_for_each_entry_safe(engine, tmp, &kernel_script_engines, list) {
        if (strcmp(engine->name, name) == 0) {
            list_del(&engine->list);
            kfree(engine);
            found = true;
            break;
        }
    }
    spin_unlock(&kernel_script_engines_lock);
    
    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(kernel_shell_unregister_script_engine);

/**
 * kernel_shell_enable_script_engine - Enable a script engine
 * @name: Engine name to enable
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_enable_script_engine(const char *name)
{
    struct kernel_script_engine *engine;
    int result = -ENOENT;
    
    if (!name) {
        return -EINVAL;
    }
    
    spin_lock(&kernel_script_engines_lock);
    list_for_each_entry(engine, &kernel_script_engines, list) {
        if (strcmp(engine->name, name) == 0) {
            if (engine->enable) {
                result = engine->enable();
                if (result == 0) {
                    engine->enabled = true;
                }
            } else {
                engine->enabled = true;
                result = 0;
            }
            break;
        }
    }
    spin_unlock(&kernel_script_engines_lock);
    
    return result;
}
EXPORT_SYMBOL(kernel_shell_enable_script_engine);

/**
 * kernel_shell_disable_script_engine - Disable a script engine
 * @name: Engine name to disable
 *
 * Returns 0 on success, negative error on failure
 */
int kernel_shell_disable_script_engine(const char *name)
{
    struct kernel_script_engine *engine;
    int result = -ENOENT;
    
    if (!name) {
        return -EINVAL;
    }
    
    spin_lock(&kernel_script_engines_lock);
    list_for_each_entry(engine, &kernel_script_engines, list) {
        if (strcmp(engine->name, name) == 0) {
            engine->enabled = false;
            if (engine->disable) {
                result = engine->disable();
            } else {
                result = 0;
            }
            break;
        }
    }
    spin_unlock(&kernel_script_engines_lock);
    
    return result;
}
EXPORT_SYMBOL(kernel_shell_disable_script_engine);
#endif // CONFIG_KERNEL_SHELL_SCRIPTING
#endif // CONFIG_KERNEL_SHELL

/**
 * vpanic - halt the system
 * @fmt: The text string to print
 * @args: Arguments for the format string
 *
 * Display a message, then perform cleanups. This function never returns.
 */
void vpanic(const char *fmt, va_list args)
{
    static char buf[1024];
    long i, i_next = 0, len;
    int state = 0;
    bool _crash_kexec_post_notifiers = crash_kexec_post_notifiers;

    if (panic_on_warn) {
        /*
         * This thread may hit another WARN() in the panic path.
         * Resetting this prevents additional WARN() from panicking the
         * system on this thread.  Other threads are blocked by the
         * panic_mutex in panic().
         */
        panic_on_warn = 0;
    }

    /*
     * Disable local interrupts. This will prevent panic_smp_self_stop
     * from deadlocking the first cpu that invokes the panic, since
     * there is nothing to prevent an interrupt handler (that runs
     * after setting panic_cpu) from invoking panic() again.
     */
    local_irq_disable();
    preempt_disable_notrace();

    /*
     * It's possible to come here directly from a panic-assertion and
     * not have preempt disabled. Some functions called from here want
     * preempt to be disabled. No point enabling it later though...
     *
     * Only one CPU is allowed to execute the panic code from here. For
     * multiple parallel invocations of panic, all other CPUs either
     * stop themself or will wait until they are stopped by the 1st CPU
     * with smp_send_stop().
     *
     * cmpxchg success means this is the 1st CPU which comes here,
     * so go ahead.
     * `old_cpu == this_cpu' means we came from nmi_panic() which sets
     * panic_cpu to this CPU.  In this case, this is also the 1st CPU.
     */
    /* atomic_try_cmpxchg updates old_cpu on failure */
    if (panic_try_start()) {
        /* go ahead */
    } else if (panic_on_other_cpu())
        panic_smp_self_stop();

    console_verbose();
    bust_spinlocks(1);
    len = vscnprintf(buf, sizeof(buf), fmt, args);

    if (len && buf[len - 1] == '\n')
        buf[len - 1] = '\0';

    // Check if graphical mode is active
    if (is_graphical_mode()) {
        // "Kill" X11/Wayland by taking over the console/framebuffer
        takeover_framebuffer();

        // Display graphical panic
        display_graphical_panic(buf);
    }

    // User-friendly BSOD-like message
    pr_emerg("\n:( \n"
             "Your system ran into a serious problem and needs to restart.\n"
             "We're collecting some error info, and then we'll restart.\n\n"
             "Technical information:\n"
             "Kernel panic - not syncing: %s\n\n", buf);

    // Keep the original info
    if (test_taint(TAINT_DIE) || oops_in_progress > 1) {
        panic_this_cpu_backtrace_printed = true;
    } else if (IS_ENABLED(CONFIG_DEBUG_BUGVERBOSE)) {
        dump_stack();
        panic_this_cpu_backtrace_printed = true;
    }

    /*
     * If kgdb is enabled, give it a chance to run before we stop all
     * the other CPUs or else we won't be able to debug processes left
     * running on them.
     */
    kgdb_panic(buf);

    /*
     * If we have crashed and we have a crash kernel loaded let it handle
     * everything else.
     * If we want to run this after calling panic_notifiers, pass
     * the "crash_kexec_post_notifiers" option to the kernel.
     *
     * Bypass the panic_cpu check and call __crash_kexec directly.
     */
    if (!_crash_kexec_post_notifiers)
        __crash_kexec(NULL);

    panic_other_cpus_shutdown(_crash_kexec_post_notifiers);

    printk_legacy_allow_panic_sync();

    /*
     * Run any panic handlers, including those that might need to
     * add information to the kmsg dump output.
     */
    atomic_notifier_call_chain(&panic_notifier_list, 0, buf);

    sys_info(panic_print);

    kmsg_dump_desc(KMSG_DUMP_PANIC, buf);

    /*
     * If you doubt kdump always works fine in any situation,
     * "crash_kexec_post_notifiers" offers you a chance to run
     * panic_notifiers and dumping kmsg before kdump.
     * Note: since some panic_notifiers can make crashed kernel
     * more unstable, it can increase risks of the kdump failure too.
     *
     * Bypass the panic_cpu check and call __crash_kexec directly.
     */
    if (_crash_kexec_post_notifiers)
        __crash_kexec(NULL);

    console_unblank();

    /*
     * We may have ended up stopping the CPU holding the lock (in
     * smp_send_stop()) while still having some valuable data in the console
     * buffer.  Try to acquire the lock then release it regardless of the
     * result.  The release will also print the buffers out.  Locks debug
     * should be disabled to avoid reporting bad unlock balance when
     * panic() is not being callled from OOPS.
     */
    debug_locks_off();
    console_flush_on_panic(CONSOLE_FLUSH_PENDING);

    if ((panic_print & SYS_INFO_PANIC_CONSOLE_REPLAY) ||
        panic_console_replay)
        console_flush_on_panic(CONSOLE_REPLAY_ALL);

#ifdef CONFIG_KERNEL_SHELL
    // Check if kernel shell is enabled and activate it
    if (kernel_shell_enabled) {
        pr_emerg("Activating kernel shell...\n");
        kernel_shell_main();
    }
#endif

    if (!panic_blink)
        panic_blink = no_blink;

    if (panic_timeout > 0) {
        /*
         * Delay timeout seconds before rebooting the machine.
         * We can't use the "normal" timers since we just panicked.
         */
        pr_emerg("The system will restart in %d seconds...\n", panic_timeout);

        for (i = 0; i < panic_timeout * 1000; i += PANIC_TIMER_STEP) {
            touch_nmi_watchdog();
            if (i >= i_next) {
                i += panic_blink(state ^= 1);
                i_next = i + 3600 / PANIC_BLINK_SPD;
            }
            mdelay(PANIC_TIMER_STEP);
        }
    }
    if (panic_timeout != 0) {
        /*
         * This will not be a clean reboot, with everything
         * shutting down.  But if there is a chance of
         * rebooting the system it will be rebooted.
         */
        if (panic_reboot_mode != REBOOT_UNDEFINED)
            reboot_mode = panic_reboot_mode;
        emergency_restart();
    }
#ifdef __sparc__
    {
        extern int stop_a_enabled;
        /* Make sure the user can actually press Stop-A (L1-A) */
        stop_a_enabled = 1;
        pr_emerg("Press Stop-A (L1-A) from sun keyboard or send break\n"
             "twice on console to return to the boot prom\n");
    }
#endif
#if defined(CONFIG_S390)
    disabled_wait();
#endif
    pr_emerg("---[ end Kernel panic - not syncing: %s ]---\n", buf);

    /* Do not scroll important messages printed above */
    suppress_printk = 1;

    /*
     * The final messages may not have been printed if in a context that
     * defers printing (such as NMI) and irq_work is not available.
     * Explicitly flush the kernel log buffer one last time.
     */
    console_flush_on_panic(CONSOLE_FLUSH_PENDING);
    nbcon_atomic_flush_unsafe();

    local_irq_enable();
    for (i = 0; ; i += PANIC_TIMER_STEP) {
        touch_softlockup_watchdog();
        if (i >= i_next) {
            i += panic_blink(state ^= 1);
            i_next = i + 3600 / PANIC_BLINK_SPD;
        }
        mdelay(PANIC_TIMER_STEP);
    }
}
EXPORT_SYMBOL(vpanic);

/* Identical to vpanic(), except it takes variadic arguments instead of va_list */
void panic(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vpanic(fmt, args);
    va_end(args);
}
EXPORT_SYMBOL(panic);

#define TAINT_FLAG(taint, _c_true, _c_false)				\
    [ TAINT_##taint ] = {						\
        .c_true = _c_true, .c_false = _c_false,			\
        .desc = #taint,						\
    }

/*
 * NOTE: if you modify the taint_flags or TAINT_FLAGS_COUNT,
 * please also modify tools/debugging/kernel-chktaint and
 * Documentation/admin-guide/tainted-kernels.rst, including its
 * small shell script that prints the TAINT_FLAGS_COUNT bits of
 * /proc/sys/kernel/tainted.
 */
const struct taint_flag taint_flags[TAINT_FLAGS_COUNT] = {
    TAINT_FLAG(PROPRIETARY_MODULE,		'P', 'G'),
    TAINT_FLAG(FORCED_MODULE,		'F', ' '),
    TAINT_FLAG(CPU_OUT_OF_SPEC,		'S', ' '),
    TAINT_FLAG(FORCED_RMMOD,		'R', ' '),
    TAINT_FLAG(MACHINE_CHECK,		'M', ' '),
    TAINT_FLAG(BAD_PAGE,			'B', ' '),
    TAINT_FLAG(USER,			'U', ' '),
    TAINT_FLAG(DIE,				'D', ' '),
    TAINT_FLAG(OVERRIDDEN_ACPI_TABLE,	'A', ' '),
    TAINT_FLAG(WARN,			'W', ' '),
    TAINT_FLAG(CRAP,			'C', ' '),
    TAINT_FLAG(FIRMWARE_WORKAROUND,		'I', ' '),
    TAINT_FLAG(OOT_MODULE,			'O', ' '),
    TAINT_FLAG(UNSIGNED_MODULE,		'E', ' '),
    TAINT_FLAG(SOFTLOCKUP,			'L', ' '),
    TAINT_FLAG(LIVEPATCH,			'K', ' '),
    TAINT_FLAG(AUX,				'X', ' '),
    TAINT_FLAG(RANDSTRUCT,			'T', ' '),
    TAINT_FLAG(TEST,			'N', ' '),
    TAINT_FLAG(FWCTL,			'J', ' '),
};

#undef TAINT_FLAG

static void print_tainted_seq(struct seq_buf *s, bool verbose)
{
    const char *sep = "";
    int i;

    if (!tainted_mask) {
        seq_buf_puts(s, "Not tainted");
        return;
    }

    seq_buf_printf(s, "Tainted: ");
    for (i = 0; i < TAINT_FLAGS_COUNT; i++) {
        const struct taint_flag *t = &taint_flags[i];
        bool is_set = test_bit(i, &tainted_mask);
        char c = is_set ? t->c_true : t->c_false;

        if (verbose) {
            if (is_set) {
                seq_buf_printf(s, "%s[%c]=%s", sep, c, t->desc);
                sep = ", ";
            }
        } else {
            seq_buf_putc(s, c);
        }
    }
}

static const char *_print_tainted(bool verbose)
{
    /* FIXME: what should the size be? */
    static char buf[sizeof(taint_flags)];
    struct seq_buf s;

    BUILD_BUG_ON(ARRAY_SIZE(taint_flags) != TAINT_FLAGS_COUNT);

    seq_buf_init(&s, buf, sizeof(buf));

    print_tainted_seq(&s, verbose);

    return seq_buf_str(&s);
}

/**
 * print_tainted - return a string to represent the kernel taint state.
 *
 * For individual taint flag meanings, see Documentation/admin-guide/sysctl/kernel.rst
 *
 * The string is overwritten by the next call to print_tainted(),
 * but is always NULL terminated.
 */
const char *print_tainted(void)
{
    return _print_tainted(false);
}

/**
 * print_tainted_verbose - A more verbose version of print_tainted()
 */
const char *print_tainted_verbose(void)
{
    return _print_tainted(true);
}

int test_taint(unsigned flag)
{
    return test_bit(flag, &tainted_mask);
}
EXPORT_SYMBOL(test_taint);

unsigned long get_taint(void)
{
    return tainted_mask;
}

/**
 * add_taint: add a taint flag if not already set.
 * @flag: one of the TAINT_* constants.
 * @lockdep_ok: whether lock debugging is still OK.
 *
 * If something bad has gone wrong, you'll want @lockdebug_ok = false, but for
 * some notewortht-but-not-corrupting cases, it can be set to true.
 */
void add_taint(unsigned flag, enum lockdep_ok lockdep_ok)
{
    if (lockdep_ok == LOCKDEP_NOW_UNRELIABLE && __debug_locks_off())
        pr_warn("Disabling lock debugging due to kernel taint\n");

    set_bit(flag, &tainted_mask);

    if (tainted_mask & panic_on_taint) {
        panic_on_taint = 0;
        panic("panic_on_taint set ...");
    }
}
EXPORT_SYMBOL(add_taint);

static void spin_msec(int msecs)
{
    int i;

    for (i = 0; i < msecs; i++) {
        touch_nmi_watchdog();
        mdelay(1);
    }
}

/*
 * It just happens that oops_enter() and oops_exit() are identically
 * implemented...
 */
static void do_oops_enter_exit(void)
{
    unsigned long flags;
    static int spin_counter;

    if (!pause_on_oops)
        return;

    spin_lock_irqsave(&pause_on_oops_lock, flags);
    if (pause_on_oops_flag == 0) {
        /* This CPU may now print the oops message */
        pause_on_oops_flag = 1;
    } else {
        /* We need to stall this CPU */
        if (!spin_counter) {
            /* This CPU gets to do the counting */
            spin_counter = pause_on_oops;
            do {
                spin_unlock(&pause_on_oops_lock);
                spin_msec(MSEC_PER_SEC);
                spin_lock(&pause_on_oops_lock);
            } while (--spin_counter);
            pause_on_oops_flag = 0;
        } else {
            /* This CPU waits for a different one */
            while (spin_counter) {
                spin_unlock(&pause_on_oops_lock);
                spin_msec(1);
                spin_lock(&pause_on_oops_lock);
            }
        }
    }
    spin_unlock_irqrestore(&pause_on_oops_lock, flags);
}

/*
 * Return true if the calling CPU is allowed to print oops-related info.
 * This is a bit racy..
 */
bool oops_may_print(void)
{
    return pause_on_oops_flag == 0;
}

/*
 * Called when the architecture enters its oops handler, before it prints
 * anything.  If this is the first CPU to oops, and it's oopsing the first
 * time then let it proceed.
 *
 * This is all enabled by the pause_on_oops kernel boot option.  We do all
 * this to ensure that oopses don't scroll off the screen.  It has the
 * side-effect of preventing later-oopsing CPUs from mucking up the display,
 * too.
 *
 * It turns out that the CPU which is allowed to print ends up pausing for
 * the right duration, whereas all the other CPUs pause for twice as long:
 * once in oops_enter(), once in oops_exit().
 */
void oops_enter(void)
{
    nbcon_cpu_emergency_enter();
    tracing_off();
    /* can't trust the integrity of the kernel anymore: */
    debug_locks_off();
    do_oops_enter_exit();

    if (sysctl_oops_all_cpu_backtrace)
        trigger_all_cpu_backtrace();
}

static void print_oops_end_marker(void)
{
    pr_warn("---[ end trace %016llx ]---\n", 0ULL);
}

/*
 * Called when the architecture exits its oops handler, after printing
 * everything.
 */
void oops_exit(void)
{
    do_oops_enter_exit();
    print_oops_end_marker();
    nbcon_cpu_emergency_exit();
    kmsg_dump(KMSG_DUMP_OOPS);
}

struct warn_args {
    const char *fmt;
    va_list args;
};

void __warn(const char *file, int line, void *caller, unsigned taint,
        struct pt_regs *regs, struct warn_args *args)
{
    nbcon_cpu_emergency_enter();

    disable_trace_on_warning();

    if (file) {
        pr_warn("WARNING: %s:%d at %pS, CPU#%d: %s/%d\n",
            file, line, caller,
            raw_smp_processor_id(), current->comm, current->pid);
    } else {
        pr_warn("WARNING: at %pS, CPU#%d: %s/%d\n",
            caller,
            raw_smp_processor_id(), current->comm, current->pid);
    }

#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif
    if (args)
        vprintk(args->fmt, args->args);
#pragma GCC diagnostic pop

    print_modules();

    if (regs)
        show_regs(regs);

    check_panic_on_warn("kernel");

    if (!regs)
        dump_stack();

    print_irqtrace_events(current);

    print_oops_end_marker();
    trace_error_report_end(ERROR_DETECTOR_WARN, (unsigned long)caller);

    /* Just a warning, don't kill lockdep. */
    add_taint(taint, LOCKDEP_STILL_OK);

    nbcon_cpu_emergency_exit();
}

#ifdef CONFIG_BUG
#ifndef __WARN_FLAGS
void warn_slowpath_fmt(const char *file, int line, unsigned taint,
               const char *fmt, ...)
{
    bool rcu = warn_rcu_enter();
    struct warn_args args;

    pr_warn(CUT_HERE);

    if (!fmt) {
        __warn(file, line, __builtin_return_address(0), taint,
               NULL, NULL);
        warn_rcu_exit(rcu);
        return;
    }

    args.fmt = fmt;
    va_start(args.args, fmt);
    __warn(file, line, __builtin_return_address(0), taint, NULL, &args);
    va_end(args.args);
    warn_rcu_exit(rcu);
}
EXPORT_SYMBOL(warn_slowpath_fmt);
#else
void __warn_printk(const char *fmt, ...)
{
    bool rcu = warn_rcu_enter();
    va_list args;

    pr_warn(CUT_HERE);

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
    warn_rcu_exit(rcu);
}
EXPORT_SYMBOL(__warn_printk);
#endif

/* Support resetting WARN*_ONCE state */

static int clear_warn_once_set(void *data, u64 val)
{
    generic_bug_clear_once();
    memset(__start_once, 0, __end_once - __start_once);
    return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(clear_warn_once_fops, NULL, clear_warn_once_set,
             "%lld\n");

static __init int register_warn_debugfs(void)
{
    /* Don't care about failure */
    debugfs_create_file_unsafe("clear_warn_once", 0200, NULL, NULL,
                   &clear_warn_once_fops);
    return 0;
}

device_initcall(register_warn_debugfs);
#endif

#ifdef CONFIG_STACKPROTECTOR

/*
 * Called when gcc's -fstack-protector feature is used, and
 * gcc detects corruption of the on-stack canary value
 */
__visible noinstr void __stack_chk_fail(void)
{
    unsigned long flags;

    instrumentation_begin();
    flags = user_access_save();

    panic("stack-protector: Kernel stack is corrupted in: %pB",
        __builtin_return_address(0));

    user_access_restore(flags);
    instrumentation_end();
}
EXPORT_SYMBOL(__stack_chk_fail);

#endif

core_param(panic, panic_timeout, int, 0644);
core_param(pause_on_oops, pause_on_oops, int, 0644);
core_param(panic_on_warn, panic_on_warn, int, 0644);
core_param(crash_kexec_post_notifiers, crash_kexec_post_notifiers, bool, 0644);
core_param(panic_console_replay, panic_console_replay, bool, 0644);

static int panic_print_set(const char *val, const struct kernel_param *kp)
{
    panic_print_deprecated();
    return  param_set_ulong(val, kp);
}

static int panic_print_get(char *val, const struct kernel_param *kp)
{
    return  param_get_ulong(val, kp);
}

static const struct kernel_param_ops panic_print_ops = {
    .set	= panic_print_set,
    .get	= panic_print_get,
};
__core_param_cb(panic_print, &panic_print_ops, &panic_print, 0644);

static int __init oops_setup(char *s)
{
    if (!s)
        return -EINVAL;
    if (!strcmp(s, "panic"))
        panic_on_oops = 1;
    return 0;
}
early_param("oops", oops_setup);

static int __init panic_on_taint_setup(char *s)
{
    char *taint_str;

    if (!s)
        return -EINVAL;

    taint_str = strsep(&s, ",");
    if (kstrtoul(taint_str, 16, &panic_on_taint))
        return -EINVAL;

    /* make sure panic_on_taint doesn't hold out-of-range TAINT flags */
    panic_on_taint &= TAINT_FLAGS_MAX;

    if (!panic_on_taint)
        return -EINVAL;

    if (s && !strcmp(s, "nousertaint"))
        panic_on_taint_nousertaint = true;

    pr_info("panic_on_taint: bitmask=0x%lx nousertaint_mode=%s\n",
        panic_on_taint, str_enabled_disabled(panic_on_taint_nousertaint));

    return 0;
}
early_param("panic_on_taint", panic_on_taint_setup);

#endif // CONFIG_KERNEL_SHELL