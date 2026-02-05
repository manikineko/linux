# Building and Using Kernel Shell Modules

## Built Modules

The following kernel shell modules have been successfully built:

### ✅ kernel_shell_test.ko
- **Purpose**: Test module for kernel shell functionality
- **Commands**: 
  - `test_input` - Test input handling in kernel shell
  - `test_kbd` - Test direct keyboard input access  
  - `test_panic` - Trigger a test panic to activate kernel shell
- **Parameters**: `auto_test` - Automatically trigger a test panic on module load
- **Usage**: Primary testing module for the new graphical PTY shell

### ✅ kernel_shell_lua.ko
- **Purpose**: Lua scripting module for kernel shell
- **Features**: Demo Lua scripting capabilities
- **Parameters**: `auto_enable` - Automatically enable Lua engine on module load
- **Usage**: Add Lua scripting support to the kernel shell

### ✅ kernel_shell_nodejs.ko
- **Purpose**: Node.js/JavaScript scripting module for kernel shell
- **Features**: Demo JavaScript scripting capabilities
- **Parameters**: `auto_enable` - Automatically enable Node.js engine on module load
- **Usage**: Add JavaScript scripting support to the kernel shell

### ✅ example_scriptable_module.ko
- **Purpose**: Example module demonstrating scriptable kernel modules
- **Features**: Shows how to make kernel modules scriptable
- **Parameters**: 
  - `debug_param` - Enable debug mode
  - `message_param` - Default message
- **Usage**: Reference implementation for scriptable modules

## Quick Start

### 1. Load the Test Module
```bash
# Load the test module
sudo insmod drivers/kernel_shell/kernel_shell_test.ko

# Verify it's loaded
lsmod | grep kernel_shell
```

### 2. Test the Graphical Shell
```bash
# Trigger a panic to activate the shell
echo c > /proc/sysrq-trigger

# You should now see the graphical shell:
# ╔══════════════════════════════════════════════════════════════════╗
# ║                    KERNEL PANIC SHELL ACTIVATED                    ║
# ║                                                                  ║
# ║  Features:                                                       ║
# ║  • Full keyboard input with shift/ctrl/alt support               ║
# ║  • Visual cursor and mouse support                                ║
# ║  • Graphical interface with ANSI colors                           ║
# ║  • Command history and editing                                    ║
# ║                                                                  ║
# ║  Type 'help' for available commands or 'continue' to shutdown     ║
# ╚══════════════════════════════════════════════════════════════════╝
#
# kernel> 
```

### 3. Test Commands
```bash
# Test input handling
kernel> test_input

# Test keyboard directly
kernel> test_kbd

# Show help
kernel> help

# Exit the shell
kernel> continue
```

### 4. Load Scripting Modules (Optional)
```bash
# Load Lua scripting
sudo insmod drivers/kernel_shell/kernel_shell_lua.ko

# Load Node.js scripting
sudo insmod drivers/kernel_shell/kernel_shell_nodejs.ko

# Load example scriptable module
sudo insmod drivers/kernel_shell/example_scriptable_module.ko
```

## Advanced Testing

### Keyboard Input Testing
```bash
# In the kernel shell, try:
kernel> help                    # Should show graphical help
kernel> !@#$%^&*()             # Should show shift symbols
kernel> hello world             # Spacebar should work
kernel>                        # Use arrow keys to navigate
```

### Mouse Testing
```bash
# Move the mouse - it should NOT interfere with typing
# Click mouse buttons - should NOT generate characters
```

### Scripting Testing
```bash
# After loading scripting modules:
kernel> script lua "print('Hello from Lua!')"
kernel> script nodejs "console.log('Hello from NodeJS!')"
```

## Module Parameters

### Test Module Parameters
```bash
# Auto-trigger panic on load (use with caution!)
sudo insmod drivers/kernel_shell/kernel_shell_test.ko auto_test=1

# Check current parameters
cat /sys/module/kernel_shell_test/parameters/auto_test
```

### Scripting Module Parameters
```bash
# Auto-enable scripting engines
sudo insmod drivers/kernel_shell/kernel_shell_lua.ko auto_enable=1
sudo insmod drivers/kernel_shell/kernel_shell_nodejs.ko auto_enable=1

# Check parameters
cat /sys/module/kernel_shell_lua/parameters/auto_enable
cat /sys/module/kernel_shell_nodejs/parameters/auto_enable
```

## Unloading Modules

```bash
# Unload modules (when not in panic mode)
sudo rmmod kernel_shell_test
sudo rmmod kernel_shell_lua
sudo rmmod kernel_shell_nodejs
sudo rmmod example_scriptable_module
```

## Troubleshooting

### Module Won't Load
```bash
# Check dmesg for errors
dmesg | tail -20

# Check kernel version compatibility
modinfo drivers/kernel_shell/kernel_shell_test.ko | grep vermagic
uname -r
```

### Shell Doesn't Activate
```bash
# Ensure kernel shell is enabled in config
grep CONFIG_KERNEL_SHELL /boot/config-$(uname -r)

# Check if panic occurred
dmesg | grep -i panic
```

### Input Not Working
```bash
# Test raw keyboard input
kernel> test_kbd

# Check for mouse interference
# Move mouse - should not affect typing
```

### Graphical Display Issues
```bash
# Check terminal supports ANSI codes
echo -e "\033[2J\033[H"

# Test basic graphical output
echo "╔══════════════════════════════════════════════════════════════════╗"
```

## Development

### Building from Source
```bash
# Clean build
make clean
make -j$(nproc) M=drivers/kernel_shell

# Build specific module
make -j$(nproc) M=drivers/kernel_shell/kernel_shell_test.ko

# Build all modules
make -j$(nproc) modules
```

### Module Development
```bash
# Use the example module as a template
cp drivers/kernel_shell/example_scriptable_module.c my_module.c

# Modify Makefile to include your module
# Add Kconfig entry for your module

# Build and test
make -j$(nproc) M=drivers/kernel_shell
```

## File Locations

Built modules are located at:
- `drivers/kernel_shell/kernel_shell_test.ko`
- `drivers/kernel_shell/kernel_shell_lua.ko`
- `drivers/kernel_shell/kernel_shell_nodejs.ko`
- `drivers/kernel_shell/example_scriptable_module.ko`

Documentation:
- `GRAPHICAL_PTY_SHELL.md` - Comprehensive feature documentation
- `TESTING_INPUT.md` - Basic testing guide
- `README.md` - General project documentation

## Safety Notes

⚠️ **WARNING**: These modules interact with kernel panic handling

1. **Test in safe environment** - Use virtual machines or test systems
2. **Backup important data** - Panic can cause system instability
3. **Don't use auto_test=1** on production systems
4. **Have recovery method** - Know how to reboot if system becomes unresponsive

## Performance Impact

- **Memory usage**: ~50KB per module loaded
- **CPU overhead**: Minimal during normal operation
- **Panic performance**: Slight delay in panic handling due to shell initialization

The graphical PTY shell provides a professional, feature-rich interface even during kernel panic conditions, with full keyboard input, visual cursor, and proper terminal emulation.
