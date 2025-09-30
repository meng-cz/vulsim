#pragma once

#include "common.h"

template <typename T>
class StorageNext {
public:

    StorageNext() {
        memset(&value, 0, sizeof(value));
        memset(&nextvalue, 0, sizeof(nextvalue));
    }
    StorageNext(const T initial) {
        value = initial;
        nextvalue = initial;
    }

    T & get() {
        return value;
    }
    void setnext(T & value, uint8 priority) {
        if(!updated || priority <= update_priority) {
            nextvalue = value;
            updated = true;
            update_priority = priority;
        }
    }
    void apply_tick() {
        if(updated) {
            value = nextvalue;
            updated = false;
            update_priority = 0;
        }
    }

protected:
    T value;
    T nextvalue;
    bool updated = false;
    uint8 update_priority = 0;
};
