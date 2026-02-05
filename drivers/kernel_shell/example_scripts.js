// Example JavaScript scripts for controlling scriptable kernel modules
// These scripts demonstrate how to interact with scriptable modules
// during kernel panic conditions using QuickJS.

// Example 1: Basic module control
function controlExampleModule() {
    console.log("=== Example Module Control ===");
    
    // Get module status
    const status = kernel.module_cmd("example_scriptable_module", "status", "");
    console.log("Module Status:");
    console.log(status);
    
    // Increment counter
    const result = kernel.module_cmd("example_scriptable_module", "increment", "5");
    console.log("Increment result:", result);
    
    // Set custom message
    const setResult = kernel.module_cmd("example_scriptable_module", "set_message", "JavaScript controlled message");
    console.log("Set message result:", setResult);
    
    // Get module info
    const info = kernel.module_cmd("example_scriptable_module", "get_info", "");
    console.log("Module Info:");
    console.log(info);
    
    // Decrement counter
    const decResult = kernel.module_cmd("example_scriptable_module", "decrement", "2");
    console.log("Decrement result:", decResult);
}

// Example 2: Module debugging
function debugExampleModule() {
    console.log("=== Module Debugging ===");
    
    // Dump debug information
    const debugInfo = kernel.module_cmd("example_scriptable_module", "debug", "dump");
    console.log("Debug Info:");
    console.log(debugInfo);
    
    // Reset module
    const resetResult = kernel.module_cmd("example_scriptable_module", "debug", "reset");
    console.log("Reset result:", resetResult);
    
    // Echo test
    const echoResult = kernel.module_cmd("example_scriptable_module", "debug", "echo Hello from JavaScript");
    console.log("Echo result:", echoResult);
}

// Example 3: Module configuration
function configureExampleModule() {
    console.log("=== Module Configuration ===");
    
    // Configure module settings
    kernel.module_cmd("example_scriptable_module", "config", "message JavaScript configured message");
    kernel.module_cmd("example_scriptable_module", "config", "debug true");
    kernel.module_cmd("example_scriptable_module", "config", "counter 100");
    
    // Check new status
    const status = kernel.module_cmd("example_scriptable_module", "status", "");
    console.log("Updated Status:");
    console.log(status);
}

// Example 4: Panic analysis with module data
function analyzeWithModuleData() {
    console.log("=== Panic Analysis with Module Data ===");
    
    // Get panic information
    kernel.panic_info();
    
    // Get system status
    kernel.system_status();
    
    // Collect data from example module
    const moduleStatus = kernel.module_cmd("example_scriptable_module", "status", "");
    console.log("Module Data:");
    console.log(moduleStatus);
    
    // Parse counter value (simplified parsing)
    const counterMatch = moduleStatus.match(/Counter: (\d+)/);
    const counter = counterMatch ? parseInt(counterMatch[1]) : 0;
    
    console.log("Analysis: Module counter =", counter);
    
    if (counter > 50) {
        console.log("WARNING: Module counter is high!");
    } else {
        console.log("Module counter is within normal range.");
    }
    
    // Create analysis report
    const report = {
        timestamp: getTime(),
        counter: counter,
        status: counter > 50 ? "HIGH" : "NORMAL",
        recommendation: counter > 50 ? "Investigate module activity" : "No action needed"
    };
    
    console.log("Analysis Report:", JSON.stringify(report, null, 2));
}

// Example 5: Automated module recovery
function attemptModuleRecovery() {
    console.log("=== Automated Module Recovery ===");
    
    // Check if module is responsive
    const status = kernel.module_cmd("example_scriptable_module", "status", "");
    
    if (status.includes("Error") || status.includes("Failed")) {
        console.log("Module appears to have issues, attempting recovery...");
        
        // Reset module
        kernel.module_cmd("example_scriptable_module", "debug", "reset");
        
        // Reconfigure with safe defaults
        kernel.module_cmd("example_scriptable_module", "config", "message Recovery completed");
        kernel.module_cmd("example_scriptable_module", "config", "debug false");
        kernel.module_cmd("example_scriptable_module", "config", "counter 0");
        
        console.log("Recovery completed");
    } else {
        console.log("Module appears to be functioning normally");
    }
}

// Example 6: Batch module operations
function batchModuleOperations() {
    console.log("=== Batch Module Operations ===");
    
    // List all scriptable modules
    const modules = kernel.modules_scriptable();
    console.log("Scriptable modules:");
    console.log(modules);
    
    // Define operations to perform
    const operations = [
        {module: "example_scriptable_module", cmd: "increment", args: "1"},
        {module: "example_scriptable_module", cmd: "set_message", args: "Batch operation completed"},
        {module: "example_scriptable_module", cmd: "increment", args: "10"}
    ];
    
    console.log("Performing batch operations...");
    
    // Execute operations with error handling
    const results = operations.map(op => {
        try {
            const result = kernel.module_cmd(op.module, op.cmd, op.args);
            return {
                ...op,
                success: true,
                result: result
            };
        } catch (error) {
            return {
                ...op,
                success: false,
                error: error.toString()
            };
        }
    });
    
    // Report results
    results.forEach((result, index) => {
        if (result.success) {
            console.log(`Operation ${index + 1} SUCCESS:`, result.result);
        } else {
            console.log(`Operation ${index + 1} FAILED:`, result.error);
        }
    });
    
    // Summary
    const successCount = results.filter(r => r.success).length;
    console.log(`Batch operations completed: ${successCount}/${results.length} successful`);
}

// Example 7: Advanced module monitoring
function monitorModuleActivity() {
    console.log("=== Advanced Module Monitoring ===");
    
    // Collect multiple data points
    const dataPoints = [];
    
    for (let i = 0; i < 5; i++) {
        const status = kernel.module_cmd("example_scriptable_module", "status", "");
        const time = getTime();
        
        // Parse counter value
        const counterMatch = status.match(/Counter: (\d+)/);
        const counter = counterMatch ? parseInt(counterMatch[1]) : 0;
        
        dataPoints.push({
            time: time,
            counter: counter,
            iteration: i + 1
        });
        
        // Small delay between measurements
        // Note: setTimeout might not work in kernel context
        // This is for demonstration purposes
    }
    
    // Analyze the data
    const counters = dataPoints.map(d => d.counter);
    const maxCounter = Math.max(...counters);
    const minCounter = Math.min(...counters);
    const avgCounter = counters.reduce((a, b) => a + b, 0) / counters.length;
    
    console.log("Monitoring Results:");
    console.log("Data Points:", JSON.stringify(dataPoints, null, 2));
    console.log(`Statistics: Min=${minCounter}, Max=${maxCounter}, Avg=${avgCounter.toFixed(2)}`);
    
    // Detect anomalies
    const anomalies = dataPoints.filter(d => d.counter > avgCounter * 1.5);
    if (anomalies.length > 0) {
        console.log("ANOMALIES DETECTED:", anomalies);
    } else {
        console.log("No anomalies detected in module activity");
    }
}

// Example 8: Module interaction patterns
function demonstrateModulePatterns() {
    console.log("=== Module Interaction Patterns ===");
    
    // Pattern 1: State validation
    console.log("Pattern 1: State Validation");
    const initialStatus = kernel.module_cmd("example_scriptable_module", "status", "");
    console.log("Initial state:", initialStatus);
    
    // Pattern 2: Transaction-like operations
    console.log("Pattern 2: Transaction Operations");
    const backup = kernel.module_cmd("example_scriptable_module", "debug", "dump");
    
    try {
        kernel.module_cmd("example_scriptable_module", "increment", "100");
        kernel.module_cmd("example_scriptable_module", "set_message", "Temporary state");
        
        const tempStatus = kernel.module_cmd("example_scriptable_module", "status", "");
        console.log("Temporary state:", tempStatus);
        
        // Rollback
        kernel.module_cmd("example_scriptable_module", "debug", "reset");
        console.log("Transaction completed and rolled back");
    } catch (error) {
        console.log("Transaction failed, restoring from backup");
        // Restore logic would go here
    }
    
    // Pattern 3: Event-driven responses
    console.log("Pattern 3: Event-Driven Response");
    const currentCounter = 42; // Would be parsed from status
    
    if (currentCounter > 40) {
        console.log("High counter detected, taking corrective action");
        kernel.module_cmd("example_scriptable_module", "decrement", "20");
        kernel.module_cmd("example_scriptable_module", "set_message", "Auto-corrected");
    }
}

// Main execution
console.log("JavaScript Module Scripting Examples");
console.log("Available functions:");
console.log("  controlExampleModule()");
console.log("  debugExampleModule()");
console.log("  configureExampleModule()");
console.log("  analyzeWithModuleData()");
console.log("  attemptModuleRecovery()");
console.log("  batchModuleOperations()");
console.log("  monitorModuleActivity()");
console.log("  demonstrateModulePatterns()");

// Uncomment to run specific examples:
// controlExampleModule();
// debugExampleModule();
// configureExampleModule();
// analyzeWithModuleData();
// attemptModuleRecovery();
// batchModuleOperations();
// monitorModuleActivity();
// demonstrateModulePatterns();

// Export functions for potential use by other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        controlExampleModule,
        debugExampleModule,
        configureExampleModule,
        analyzeWithModuleData,
        attemptModuleRecovery,
        batchModuleOperations,
        monitorModuleActivity,
        demonstrateModulePatterns
    };
}
