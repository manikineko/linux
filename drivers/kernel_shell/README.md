# Kernel Shell Implementation

This directory contains the implementation of a kernel shell that provides debugging and recovery capabilities during panic conditions, along with scripting support via Lua and Node.js modules.

## Overview

The kernel shell is activated during panic conditions and provides:
- Interactive shell interface during kernel panics
- Logo display during panic (configurable)
- Extensible command system via kernel modules
- Scripting support with Lua and Node.js engines
- Dynamic enable/disable of scripting engines

## Features

### Core Features
- **Kernel Shell**: Minimal shell accessible during panic conditions
- **Logo Display**: ASCII art logo shown during panic (configurable)
- **Command System**: Built-in commands and extensible via modules
- **Scripting Support**: Lua and Node.js scripting engines
- **Module Interface**: API for extending shell functionality

### Built-in Commands
- `help` - Show available commands
- `continue` - Continue with panic shutdown
- `status` - Show system status information
- `modules` - List loaded modules
- `script` - Execute a script (usage: script <engine> <script>)
- `engines` - List available script engines
- `modules_scriptable` - List all scriptable modules
- `module_cmd` - Execute module command (usage: module_cmd <module> <cmd> [args])
- `module_script` - Execute script in module context (usage: module_script <module> <engine> <script>)

### Scripting Engines
- **Lua**: Lua scripting support via kernel_shell_lua module (demo) or kernel_shell_lua_real module (full Lua 5.5.0)
- **Node.js**: JavaScript scripting support via kernel_shell_nodejs module (demo) or kernel_shell_nodejs_real module (QuickJS)

#### Demo vs Real Implementations
- **Demo modules** (`kernel_shell_lua`, `kernel_shell_nodejs`): Simplified implementations with basic command parsing
- **Real modules** (`kernel_shell_lua_real`, `kernel_shell_nodejs_real`): Full language integration with actual interpreters

#### Real Lua Implementation
- **Engine**: Lua 5.5.0 interpreter
- **Features**: Full Lua language support, standard libraries, kernel API
- **Configuration**: `CONFIG_KERNEL_SHELL_LUA_REAL_MODULE`
- **Memory**: Kernel-space memory management with optional debugging
- **Safety**: File I/O and system calls disabled for kernel safety

#### Real QuickJS Implementation  
- **Engine**: QuickJS 2024-01-13 (ES2020 compliant)
- **Features**: Full JavaScript support, Node.js-like console API, kernel API
- **Configuration**: `CONFIG_KERNEL_SHELL_NODEJS_REAL_MODULE`
- **Memory**: Configurable memory limits, kernel-space allocation
- **Safety**: Network, file, and process operations disabled for kernel safety

### Module Scripting System

The kernel shell includes a comprehensive module scripting system that allows kernel modules to be controlled and configured via Lua and JavaScript scripts during panic conditions.

#### Features
- **Module Registration**: Modules can register for scripting control
- **Custom Commands**: Modules can expose custom scripting commands
- **Standard Operations**: Built-in operations (init, exit, suspend, resume, config, status, debug)
- **Script Context**: Scripts can execute in module-specific contexts
- **Error Handling**: Robust error handling and reporting

#### Module Scripting API
```c
// Register module for scripting
int kernel_module_register_scripting(struct module *mod,
                                     const struct kernel_module_script_ops *ops);

// Register custom commands
int kernel_module_register_script_cmd(struct module *mod,
                                      const char *name,
                                      const char *description,
                                      int (*execute)(const char *args, char *output, size_t output_size));

// Execute module commands
int kernel_module_execute_script_cmd(struct module *mod,
                                    const char *cmd,
                                    const char *args,
                                    char *output,
                                    size_t output_size);
```

#### Module Scripting Commands
- `modules_scriptable` - List all scriptable modules
- `module_cmd <module> <cmd> [args]` - Execute module command
- `module_script <module> <engine> <script>` - Execute script in module context

#### Example Usage
```bash
# List scriptable modules
kernel> modules_scriptable

# Execute module command
kernel> module_cmd example_scriptable_module increment 5

# Execute script in module context
kernel> module_script example_scriptable_module lua "print('Module script executed')"
```

#### Example Scriptable Module
The `example_scriptable_module` demonstrates the scripting interface:
- Custom commands: `increment`, `decrement`, `set_message`, `get_info`
- Standard operations: `status`, `config`, `debug`
- Proc file interface for monitoring
- Lua and JavaScript integration examples

## Configuration Options

### Kernel Configuration
```
CONFIG_KERNEL_SHELL              - Enable kernel shell support
CONFIG_KERNEL_SHELL_LOGO         - Display logo during panic
CONFIG_KERNEL_SHELL_SCRIPTING    - Enable scripting support
CONFIG_KERNEL_SHELL_LUA          - Lua scripting support
CONFIG_KERNEL_SHELL_NODEJS       - Node.js scripting support
```

### Module Configuration
```
CONFIG_KERNEL_SHELL_LUA_MODULE           - Build demo Lua scripting module
CONFIG_KERNEL_SHELL_LUA_REAL_MODULE      - Build real Lua scripting module
CONFIG_KERNEL_SHELL_NODEJS_MODULE        - Build demo Node.js scripting module
CONFIG_KERNEL_SHELL_NODEJS_REAL_MODULE   - Build real QuickJS scripting module
CONFIG_EXAMPLE_SCRIPTABLE_MODULE         - Build example scriptable module
```

## File Structure

```
drivers/kernel_shell/
├── Kconfig                      # Configuration options
├── Makefile                      # Build configuration
├── README.md                     # This documentation
├── build.sh                      # Build automation script
├── kernel_shell_lua.c           # Demo Lua scripting module
├── kernel_shell_nodejs.c        # Demo Node.js scripting module
├── kernel_shell_lua_real.c      # Real Lua scripting module
├── kernel_shell_nodejs_real.c   # Real QuickJS scripting module
├── example_scriptable_module.c  # Example scriptable module
├── example_scripts.lua          # Lua scripting examples
├── example_scripts.js           # JavaScript scripting examples
├── lua_kernel_adapter.h         # Lua kernel space adapter
├── lua_kernel_patch.c           # Lua kernel compatibility patches
├── quickjs_kernel_adapter.h    # QuickJS kernel space adapter
├── quickjs_kernel_patch.c       # QuickJS kernel compatibility patches
├── lua-5.5.0/                   # Lua 5.5.0 source code
└── quickjs-2024-01-13/          # QuickJS source code

include/linux/kernel_shell.h           # Kernel shell interface header
include/linux/kernel_shell_module.h    # Module scripting interface header
kernel/panic.c                         # Modified panic function with shell integration
kernel/ks_module_scripting.c           # Module scripting implementation
lib/Kconfig.debug                      # Added kernel shell config options
```

## Usage

### Enabling the Kernel Shell

1. Enable kernel shell in kernel configuration:
   ```
   CONFIG_KERNEL_SHELL=y
   CONFIG_KERNEL_SHELL_LOGO=y
   CONFIG_KERNEL_SHELL_SCRIPTING=y
   ```

2. Enable scripting engines:
   ```
   CONFIG_KERNEL_SHELL_LUA=y
   CONFIG_KERNEL_SHELL_NODEJS=y
   ```

3. Build modules:
   ```
   CONFIG_KERNEL_SHELL_LUA_MODULE=m           # Demo Lua module
   CONFIG_KERNEL_SHELL_LUA_REAL_MODULE=m      # Real Lua module
   CONFIG_KERNEL_SHELL_NODEJS_MODULE=m         # Demo Node.js module
   CONFIG_KERNEL_SHELL_NODEJS_REAL_MODULE=m   # Real QuickJS module
   ```

### Loading Modules

```bash
# Load demo modules
modprobe kernel_shell_lua
modprobe kernel_shell_nodejs

# Load real modules (recommended for production use)
modprobe kernel_shell_lua_real
modprobe kernel_shell_nodejs_real
```

### Using the Shell

During a kernel panic, the shell will automatically activate if enabled. The shell provides:

1. **Logo Display**: Shows kernel panic logo
2. **Command Prompt**: `kernel>` prompt for commands
3. **Help System**: Type `help` for available commands
4. **Script Execution**: Use `script <engine> <code>` to execute scripts

### Script Examples

#### Lua Script (Real Implementation)
```lua
-- Full Lua language support available
print("Hello from real Lua in Kernel Shell!")
panic_info()  -- Get panic information

-- Use Lua standard libraries
local table = require "table"
local data = {cpu = raw_smp_processor_id(), pid = current.pid}
print("System info:", table.concat(data, ", "))

-- Control structures
for i = 1, 5 do
    print("Count:", i)
    if i == 3 then
        system_status()
        break
    end
end
```

#### Node.js Script (QuickJS Implementation)
```javascript
// Full JavaScript/ES2020 support available
console.log('Hello from real QuickJS in Kernel Shell!');
kernel.panic_info();  // Get panic information

// Use modern JavaScript features
const info = {
    cpu: raw_smp_processor_id(),
    pid: current.pid,
    time: getTime()
};
console.log('System info:', JSON.stringify(info));

// Arrow functions and closures
const processData = (data) => {
    return data.map(x => x * 2).filter(x => x > 10);
};

// Async-like patterns (simplified for kernel)
setTimeout(() => {
    console.log('Delayed execution');
}, 1000);
```

#### Demo Script Examples
For demo modules, use simplified syntax:
```lua
-- Demo Lua
print("Hello from Lua demo")
kernel.panic_info()
```

```javascript
// Demo Node.js
console.log('Hello from Node.js demo');
kernel.getPanicInfo();
```

## Module API

### Command Registration

```c
int kernel_shell_register_command(const char *name, 
                                  void (*func)(const char *), 
                                  const char *help);
```

### Script Engine Registration

```c
int kernel_shell_register_script_engine(const char *name,
                                        int (*execute)(const char *),
                                        int (*enable)(void),
                                        int (*disable)(void));
```

### Engine Control

```c
int kernel_shell_enable_script_engine(const char *name);
int kernel_shell_disable_script_engine(const char *name);
```

## Security Considerations

- Scripts run in kernel context with full privileges
- Script engines should implement proper sandboxing
- Module loading should be restricted to privileged users
- Consider adding script validation and execution limits

## Implementation Notes

### Current Implementation
- **Simplified Script Execution**: Current implementation parses basic commands
- **Demo Mode**: Shell auto-executes demo commands for testing
- **Stub Functions**: Some functions are simplified for demonstration

### Production Enhancements
For production use, consider implementing:
- Real Lua and JavaScript engine integration
- Proper input handling and keyboard support
- Enhanced security and sandboxing
- More comprehensive kernel API access
- Persistent script storage
- Network-based debugging capabilities

## Integration Points

### Modified Files
- `kernel/panic.c`: Added shell integration to panic function
- `lib/Kconfig.debug`: Added kernel shell configuration options
- `drivers/Kconfig`: Added kernel shell menu
- `drivers/Makefile`: Added kernel shell build support

### New Files
- `include/linux/kernel_shell.h`: Public interface header
- `drivers/kernel_shell/`: Module implementations

## Troubleshooting

### Common Issues

1. **Shell not activating**: Check `CONFIG_KERNEL_SHELL` is enabled
2. **Modules not loading**: Verify module dependencies and signatures
3. **Script engines not available**: Check scripting configuration
4. **Commands not found**: Verify modules are loaded and commands registered

### Debug Information

Enable kernel logging to see shell activation messages:
```bash
dmesg | grep kernel_shell
```

## Contributing

When extending the kernel shell:
1. Follow kernel coding standards
2. Implement proper error handling
3. Consider security implications
4. Add appropriate configuration options
5. Update documentation

## License

This implementation is licensed under GPL-2.0 as required for kernel code.
