#pragma once

#include "common.h"

template <typename T>
class PipeInputPort {
public:
    bool can_pop() {
        return valid;
    }
    T & top() {
        return value;
    }
    void pop() {
        accepted = true;
    }

    bool valid = false;
    T value;
    bool accepted = false;
};
template <typename T>
class PipeOutputPort {
public:
    bool can_push() {
        return ready;
    }
    void push(const T & value) {
        accepted = true;
        this->value = value;
    }
    bool ready = false;
    T value;
    bool accepted = false;
};


template <typename T, int Depth = 0, int InNum = 1, int OutNum = 1>
class Pipe {
public:

static_assert(InNum > 0 && OutNum > 0, "Pipe input_num and output_num must be greater than 0");
static_assert(Depth >= 0, "Pipe depth must be non-negative");

    array<PipeInputPort<T>, OutNum> outputs;
    array<PipeOutputPort<T>, InNum> inputs;

    void apply_tick() {
        uint32 remained_output = 0;
        for(uint32 i = 0; i < OutNum; i++) {
            if(outputs[i].valid && outputs[i].accepted) {
                outputs[i].valid = false;
                outputs[i].accepted = false;
            }
            else if(outputs[i].valid) {
                outputs[remained_output].valid = true;
                outputs[remained_output].value = outputs[i].value;
                outputs[remained_output].accepted = false;
                remained_output++;
            }
        }

        // 1. 处理输入端，将数据推进fifo
        for (auto &input : inputs) {
            if (input.ready && input.accepted) {
                fifo.push_back(input.value);
                input.accepted = false;
            }
        }

        // 2. 处理输出端，从fifo分发数据
        for(uint32 i = remained_output; i < OutNum; i++) {
            if(!fifo.empty()) {
                outputs[i].valid = true;
                outputs[i].value = fifo.front();
                fifo.pop_front();
            } else {
                outputs[i].valid = false;
            }
        }
        // 3. 更新输入端ready信号（fifo未满才可写）
        uint32 valid_input = (Depth > fifo.size()) ? (Depth - fifo.size()) : 0;
        for(uint32 i = 0; i < InNum; i++) {
            inputs[i].ready = (i < valid_input);
        }
    }

protected:
    list<T> fifo;
}; 

