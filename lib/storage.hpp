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

template <typename T>
class StorageNextArray {
public:

    StorageNextArray() {}
    
    StorageNextArray(int64 size) {
        data.resize(size);
    }
    StorageNextArray(int64 size, const T initial) {
        data.assign(size, StorageNext<T>(initial));
    }
    StorageNextArray(int64 size, const T * initial) {
        data.resize(size);
        for(int64 i = 0; i < size; i++) {
            data[i] = StorageNext<T>(initial[i]);
        }
    }

    T & get(int64 index) {
        return data[index].get();
    }
    void setnext(int64 index, T & value, uint8 priority) {
        data[index].setnext(value, priority);
    }
    void apply_tick() {
        for(auto &item : data) {
            item.apply_tick();
        }
    }

protected:
    vector<StorageNext<T>> data;
};
