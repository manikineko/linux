-- Example Lua scripts for controlling scriptable kernel modules
-- These scripts demonstrate how to interact with scriptable modules
-- during kernel panic conditions.

-- Example 1: Basic module control
function control_example_module()
    print("=== Example Module Control ===")
    
    -- Get module status
    local status = kernel.module_cmd("example_scriptable_module", "status", "")
    print("Module Status:")
    print(status)
    
    -- Increment counter
    local result = kernel.module_cmd("example_scriptable_module", "increment", "5")
    print("Increment result:", result)
    
    -- Set custom message
    result = kernel.module_cmd("example_scriptable_module", "set_message", "Lua controlled message")
    print("Set message result:", result)
    
    -- Get module info
    local info = kernel.module_cmd("example_scriptable_module", "get_info", "")
    print("Module Info:")
    print(info)
    
    -- Decrement counter
    result = kernel.module_cmd("example_scriptable_module", "decrement", "2")
    print("Decrement result:", result)
end

-- Example 2: Module debugging
function debug_example_module()
    print("=== Module Debugging ===")
    
    -- Dump debug information
    local debug_info = kernel.module_cmd("example_scriptable_module", "debug", "dump")
    print("Debug Info:")
    print(debug_info)
    
    -- Reset module
    local reset_result = kernel.module_cmd("example_scriptable_module", "debug", "reset")
    print("Reset result:", reset_result)
    
    -- Echo test
    local echo_result = kernel.module_cmd("example_scriptable_module", "debug", "echo Hello from Lua")
    print("Echo result:", echo_result)
end

-- Example 3: Module configuration
function configure_example_module()
    print("=== Module Configuration ===")
    
    -- Configure module settings
    kernel.module_cmd("example_scriptable_module", "config", "message Lua configured message")
    kernel.module_cmd("example_scriptable_module", "config", "debug true")
    kernel.module_cmd("example_scriptable_module", "config", "counter 100")
    
    -- Check new status
    local status = kernel.module_cmd("example_scriptable_module", "status", "")
    print("Updated Status:")
    print(status)
end

-- Example 4: Panic analysis with module data
function analyze_with_module_data()
    print("=== Panic Analysis with Module Data ===")
    
    -- Get panic information
    panic_info()
    
    -- Get system status
    system_status()
    
    -- Collect data from example module
    local module_status = kernel.module_cmd("example_scriptable_module", "status", "")
    print("Module Data:")
    print(module_status)
    
    -- Perform analysis
    local counter = 0  -- Extract counter from status (simplified)
    print("Analysis: Module counter =", counter)
    
    if counter > 50 then
        print("WARNING: Module counter is high!")
    else
        print("Module counter is within normal range.")
    end
end

-- Example 5: Automated module recovery
function attempt_module_recovery()
    print("=== Automated Module Recovery ===")
    
    -- Check if module is responsive
    local status = kernel.module_cmd("example_scriptable_module", "status", "")
    
    if string.find(status, "Error") or string.find(status, "Failed") then
        print("Module appears to have issues, attempting recovery...")
        
        -- Reset module
        kernel.module_cmd("example_scriptable_module", "debug", "reset")
        
        -- Reconfigure with safe defaults
        kernel.module_cmd("example_scriptable_module", "config", "message Recovery completed")
        kernel.module_cmd("example_scriptable_module", "config", "debug false")
        kernel.module_cmd("example_scriptable_module", "config", "counter 0")
        
        print("Recovery completed")
    else
        print("Module appears to be functioning normally")
    end
end

-- Example 6: Batch module operations
function batch_module_operations()
    print("=== Batch Module Operations ===")
    
    -- List all scriptable modules
    local modules = kernel.modules_scriptable()
    print("Scriptable modules:")
    print(modules)
    
    -- Perform operations on all available modules
    -- (This would require parsing the module list)
    print("Performing batch operations...")
    
    -- Example: increment counters in multiple modules
    local operations = {
        {module = "example_scriptable_module", cmd = "increment", args = "1"},
        {module = "example_scriptable_module", cmd = "set_message", args = "Batch operation completed"}
    }
    
    for _, op in ipairs(operations) do
        local result = kernel.module_cmd(op.module, op.cmd, op.args)
        print(string.format("Operation on %s: %s", op.module, result))
    end
end

-- Main execution
print("Lua Module Scripting Examples")
print("Available functions:")
print("  control_example_module()")
print("  debug_example_module()")
print("  configure_example_module()")
print("  analyze_with_module_data()")
print("  attempt_module_recovery()")
print("  batch_module_operations()")

-- Uncomment to run specific examples:
-- control_example_module()
-- debug_example_module()
-- configure_example_module()
-- analyze_with_module_data()
-- attempt_module_recovery()
-- batch_module_operations()
