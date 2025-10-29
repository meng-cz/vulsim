#pragma once

#include "common.h"
#include "global.h"
#include "bundle.h"
#include "vulsimlib.h"

// Combine: SimpleMemoryBlock64
// 
class SimpleMemoryBlock64 {
public:
    class ConstructorArguments {
    public:
        int64 BaseAddress;
        int64 SizeMegaBytes;
        string __instance_name;
    };

    string __instance_name;

    SimpleMemoryBlock64(ConstructorArguments & arg);
    ~SimpleMemoryBlock64();

    void all_current_tick();
    void all_current_applytick();

    /*
     * memory_load
     * @arg <address> address
     * @ret <data> data
     */
    void memory_load(uint64 address, uint64 * data);
    
    /*
     * memory_store
     * @arg <address> address
     * @arg <data> data
     */
    void memory_store(uint64 address, uint64 data);
    
    // Base address for this memory block
    int64 BaseAddress;
    // Size of this memory block in MB
    int64 SizeMegaBytes;

protected:

    vector<uint64> __block_data;

    void initialize_from_json();
};


