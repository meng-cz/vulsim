# Simulation.cpp 代码生成

## 整体框架

```cpp

#include "simulation.h"
#include "common.h"
#include "global.h"
#include "bundle.h"
#include "vulsimlib.h"

#include <memory>
using std::unique_ptr;
using std::make_unique;

// Include Field
// To include all combine.h
// #include "$name$.h"

// Instance Field
// To store all instances with a unique_ptr for each
// unique_ptr<$combine$> _instance_$name$;

// Pipe Field
// To store all pipes with a unique_ptr for each
// unique_ptr<Pipe<$type$, $buf$, $in$, $out$>> _pipe_$name$;

// Req Connection Field
// For each instance.request a global function is setup to call all connected service
// void _request_$instname$_$reqname$($argtype$ (&) $argname$, ... , $rettype$ * $retname$, ... ) {
//     _instance_$servinstname$->$servname$($argname$, ... , $retname$, ...);
//     ......
// }

// Stalled Connection Field
// For each stallable instance a global function for _external_stall() is setup to call all connected stall()
// void _stall_$instname$() {
//     _instance_$dstinstname$->stall();
// }

void init_simulation() {
    // Pipe Init Field
    // Call constructors for pipes
    // _pipe_$name$ = make_unique<Pipe<$type$, $buf$, $in$, $out$>>();

    // Instance Init Field
    // Call constructor for instance
    // {
    //     $combine$::ConstructorArguments args;
    //     // for pipein: args._pipein_$name$ = &(_pipe_$copipename$->outputs[$copipeport]);
    //     // for pipeout: args._pipeout_$name$ = &(_pipe_$copipename$->inputs[$copipeport]);
    //     // for request: args._request_$name$ = _request_$instname$_$name$;
    //     // for stallable: args._external_stall = _stall_$instname$;
    //     // for config: args.$name$ = $GEN_CONFIG_STATEMENT$;
    //     _instance_$name$ = make_unique<$combine$>(args);
    // }
}

void _sim_tick() {
    // Tick Field
    // Update each instance according to update sequence
    // _instance_$name$->all_current_tick();
}

void _sim_applytick() {
    // ApplyTick Field
    // Update each instance according to update sequence
    // _instance_$name$->all_current_applytick();

    // ApplyTick Pipe Field
    // Update each pipe
    // _pipe_$name$->apply_tick();

}

void run_simulation_tick() {
    _sim_tick();
    _sim_applytick();
}

void finalize_simulation() {
    // Finalize Field
    // Reset all instance pointer
    // _instance_$name$.reset();
    
    // Finalize Pipe Field
    // Reset all pipe pointer
    // _pipe_$name$.reset();
};


```

