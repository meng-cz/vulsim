#pragma once

#include "common.h"

template <typename T>
class PipePopPort {
public:
    bool can_pop() {
        return *valid;
    }
    T & top() {
        return *value;
    }
    void pop() {
        if (!(*valid)) return;
        *valid = false;
        *accepted = true;
    }

    bool *valid;
    T *value;
    bool *accepted;
};
template <typename T>
class PipePushPort {
public:
    bool can_push() {
        return *ready;
    }
    void push(const T & value) {
        if (!(*ready)) return;
        *accepted = true;
        *ready = false;
        *(this->value) = value;
    }
    bool *ready;
    T *value;
    bool *accepted;
};


template <typename T>
class SimpleHandshakePipe {

public:
    SimpleHandshakePipe() {
        outputs[0].valid = &pop_valid;
        outputs[0].value = &pop_buf;
        outputs[0].accepted = &pop_accepted;
        inputs[0].ready = &push_ready;
        inputs[0].accepted = &push_accepted;
        inputs[0].value = &push_buf;
    }

    array<PipePopPort<T>, 1> outputs;
    array<PipePushPort<T>, 1> inputs;

    void apply_tick() {
        if (clear_flag) {
            clear_flag = false;
            pop_valid = false;
            pop_accepted = false;
            push_ready = true;
            push_accepted = false;
            return;
        }

        if (push_accepted && !pop_valid) {
            pop_valid = true;
            pop_buf = push_buf;
            push_ready = true;
            push_accepted = false;
        }
    }

    void clear() {
        clear_flag = true;
    }

protected:
    bool clear_flag = false;
    bool pop_valid = false;
    bool pop_accepted = false;
    T pop_buf;
    bool push_ready = true;
    bool push_accepted = false;
    T push_buf;
};

template <typename T>
class SimpleNonHandshakePipe {

public:

    SimpleNonHandshakePipe() {
        outputs[0].valid = &pop_valid;
        outputs[0].value = &pop_buf;
        outputs[0].accepted = &pop_accepted;
        inputs[0].ready = &push_ready;
        inputs[0].accepted = &push_accepted;
        inputs[0].value = &push_buf;
    }

    array<PipePopPort<T>, 1> outputs;
    array<PipePushPort<T>, 1> inputs;

    void apply_tick() {
        if (clear_flag) {
            clear_flag = false;
            pop_valid = false;
            pop_accepted = false;
            push_ready = true;
            push_accepted = false;
            return;
        }

        pop_accepted = false;
        if (push_accepted) {
            pop_valid = true;
            pop_buf = push_buf;
        }

        // always ready to accept new data
        push_ready = true;
    }

    void clear() {
        clear_flag = true;
    }

protected:
    bool clear_flag = false;
    bool pop_valid = false;
    bool pop_accepted = false;
    T pop_buf;
    bool push_ready = true;
    bool push_accepted = false;
    T push_buf;
};

template <typename T, int Depth = 1>
class SimpleBufferedPipe {

public:

    static_assert(Depth > 1, "Buffered pipe depth must be greater than 1");

    SimpleBufferedPipe() {
        outputs[0].valid = &pop_valid;
        outputs[0].value = &pop_buf;
        outputs[0].accepted = &pop_accepted;
        inputs[0].ready = &push_ready;
        inputs[0].accepted = &push_accepted;
        inputs[0].value = &push_buf;
    }

    array<PipePopPort<T>, 1> outputs;
    array<PipePushPort<T>, 1> inputs;

    void apply_tick() {
        if (clear_flag) {
            clear_flag = false;
            pop_valid = false;
            pop_accepted = false;
            push_ready = true;
            push_accepted = false;
            fifo.clear();
            return;
        }

        if (!fifo.empty() || push_accepted) {
            if (!pop_valid) {
                pop_valid = true;
                if (!fifo.empty()) {
                    pop_buf = fifo.front();
                    fifo.pop_front();
                } else {
                    pop_buf = push_buf;
                    push_accepted = false;
                    push_ready = true;
                }
            }
        }
        if (push_accepted && fifo.size() < Depth) {
            fifo.push_back(push_buf);
            push_accepted = false;
            push_ready = true;
        }

    }

    void clear() {
        clear_flag = true;
    }

protected:
    bool clear_flag = false;
    bool pop_valid = false;
    bool pop_accepted = false;
    T pop_buf;
    bool push_ready = true;
    bool push_accepted = false;
    T push_buf;
    list<T> fifo;
};

// template <typename T, int Depth = 0, int InNum = 1, int OutNum = 1>
// class BufferedHandshakePipe {
// public:

// static_assert(InNum > 0 && OutNum > 0, "Pipe input_num and output_num must be greater than 0");
// static_assert(Depth >= 0, "Pipe depth must be non-negative");

//     array<PipePopPort<T>, OutNum> outputs;
//     array<PipePushPort<T>, InNum> inputs;

//     void apply_tick() {
//         if (clear_flag) {
//             clear_flag = false;
//             fifo.clear();
//             for(uint32 i = 0; i < OutNum; i++) {
//                 outputs[i].valid = false;
//                 outputs[i].accepted = false;
//             }
//             for(uint32 i = 0; i < InNum; i++) {
//                 inputs[i].ready = true;
//                 inputs[i].accepted = false;
//             }
//             return;
//         }
        

//         uint32 remained_output = 0;
//         for(uint32 i = 0; i < OutNum; i++) {
//             if(outputs[i].valid && outputs[i].accepted) {
//                 outputs[i].valid = false;
//                 outputs[i].accepted = false;
//             }
//             else if(outputs[i].valid) {
//                 outputs[remained_output].valid = true;
//                 outputs[remained_output].value = outputs[i].value;
//                 outputs[remained_output].accepted = false;
//                 remained_output++;
//             }
//         }

//         // 1. 处理输入端，将数据推进fifo
//         for (auto &input : inputs) {
//             if (input.ready && input.accepted) {
//                 fifo.push_back(input.value);
//                 input.accepted = false;
//             }
//         }

//         // 2. 处理输出端，从fifo分发数据
//         for(uint32 i = remained_output; i < OutNum; i++) {
//             if(!fifo.empty()) {
//                 outputs[i].valid = true;
//                 outputs[i].value = fifo.front();
//                 fifo.pop_front();
//             } else {
//                 outputs[i].valid = false;
//             }
//         }
//         // 3. 更新输入端ready信号（fifo未满才可写）
//         uint32 valid_input = ((Depth + InNum) > fifo.size()) ? ((Depth + InNum) - fifo.size()) : 0;
//         for(uint32 i = 0; i < InNum; i++) {
//             inputs[i].ready = (i < valid_input);
//         }
//     }

//     void clear() {
//         clear_flag = true;
//     }

// protected:
//     bool clear_flag = false;
//     list<T> fifo;

// }; 

