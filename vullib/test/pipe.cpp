#include "pipe.hpp"

#include <iostream>
#include <cassert>

using namespace vulpipe;

void test_basic() {
    PipeNoBlockMultiPort<int, 1, 1> pipe;
    int data;

    // Initial state
    assert(!pipe.can_pop());
    assert(pipe.can_push());

    // Push data
    assert(pipe.push(10));
    assert(!pipe.can_push()); // Cannot push again this tick

    // Advance to next tick
    pipe.apply_next_tick();
    assert(pipe.can_pop()); // Ready after 1 tick (Depth=1)
    assert(pipe.pop(data));
    assert(data == 10);
    assert(!pipe.can_pop()); // Cannot pop again this tick
}

void test_multiple_pushes() {
    PipeNoBlockMultiPort<int, 1, 1> pipe;
    int data;

    // Push first
    assert(pipe.push(1));
    assert(!pipe.can_push());

    // Try push again same tick - should fail
    assert(!pipe.push(2));

    pipe.apply_next_tick();
    assert(pipe.can_pop());
    assert(pipe.pop(data));
    assert(data == 1);

    // Now can push again
    assert(pipe.can_push());
    assert(pipe.push(3));
    pipe.apply_next_tick();
    assert(pipe.pop(data));
    assert(data == 3);
}

void test_clear() {
    PipeNoBlockMultiPort<int, 1, 1> pipe;
    int data;

    // Push and clear same tick
    assert(pipe.push(1));
    pipe.clear();
    // Same tick push should be discarded
    assert(!pipe.push(2));

    pipe.apply_next_tick();
    // Should be cleared, no data
    assert(!pipe.can_pop());

    // Now push should work
    assert(pipe.push(3));
    pipe.apply_next_tick();
    assert(pipe.pop(data));
    assert(data == 3);
}

void test_clear_after_push() {
    PipeNoBlockMultiPort<int, 2, 1> pipe;
    int data;

    assert(pipe.push(1));
    pipe.apply_next_tick();
    pipe.clear();
    pipe.apply_next_tick();
    // Should be cleared
    assert(!pipe.can_pop());
}

void test_multi_port() {
    PipeNoBlockMultiPort<int, 1, 2> pipe;
    int data;

    // Initial state
    assert(!pipe.can_pop());
    assert(pipe.can_push());

    // Push two data in one tick
    assert(pipe.push(10));
    assert(pipe.push(20));
    assert(!pipe.can_push()); // Width=2, cannot push more

    // Advance to next tick
    pipe.apply_next_tick();
    assert(pipe.can_pop());

    // Pop two data
    assert(pipe.pop(data)); assert(data == 10);
    assert(pipe.pop(data)); assert(data == 20);
    assert(!pipe.can_pop()); // Cannot pop more this tick
}

void test_pipe_depth1() {
    PipeDepth1<int> pipe;
    int data;

    // Initial state
    assert(pipe.can_push());
    assert(!pipe.can_pop());

    // Push data
    assert(pipe.push(42));
    assert(!pipe.can_push()); // Cannot push again this tick

    // Advance to next tick
    pipe.apply_next_tick();
    assert(pipe.can_pop());

    // Pop data
    assert(pipe.pop(data));
    assert(data == 42);
    assert(!pipe.can_pop()); // Cannot pop again this tick
    assert(pipe.can_push()); // Now can push

    // Push again
    assert(pipe.push(43));
    pipe.apply_next_tick();
    assert(pipe.pop(data));
    assert(data == 43);

    // Test clear
    assert(pipe.push(44));
    pipe.apply_next_tick();
    pipe.clear();
    assert(!pipe.can_push());
    pipe.apply_next_tick();
    assert(!pipe.can_pop());
    assert(pipe.can_push());
    pipe.apply_next_tick();
    assert(!pipe.can_pop());
    pipe.apply_next_tick();

    // Test stall
    assert(pipe.push(46));
    pipe.apply_next_tick(); // Data is ready
    assert(pipe.can_pop());
    // Simulate stall by not popping
    assert(!pipe.can_push());
    pipe.apply_next_tick(); // Data should still be there
    assert(pipe.can_pop());
    assert(pipe.pop(data));
    assert(data == 46);
    assert(pipe.can_push());

}

void test_pipe_buffered_depth1() {
    PipeBufferedDepth1<int, 2> pipe;
    int data;

    // Initial state
    assert(pipe.can_push());
    assert(!pipe.can_pop());

    // Push first data, should go to pop_buffer
    assert(pipe.push(10));
    assert(!pipe.can_push()); // Cannot push again this tick
    pipe.apply_next_tick();
    assert(pipe.can_pop());
    assert(pipe.pop(data));
    assert(data == 10);
    assert(!pipe.can_pop());
    assert(pipe.can_push());

    // Push second data, queue empty, should go to pop_buffer
    assert(pipe.push(20));
    pipe.apply_next_tick();
    assert(pipe.pop(data));
    assert(data == 20);

    // Push two data in sequence
    assert(pipe.push(30));
    pipe.apply_next_tick(); // 30 to pop_buffer
    assert(pipe.can_pop());
    assert(pipe.push(40)); // 40 to queue
    pipe.apply_next_tick(); // 40 to queue
    assert(pipe.pop(data)); // pop 30
    assert(data == 30);
    pipe.apply_next_tick(); // 40 to queue
    assert(pipe.can_pop()); // 40 in queue
    assert(pipe.pop(data));
    assert(data == 40);
    pipe.apply_next_tick(); // 40 to queue

    // Test clear
    assert(pipe.push(50));
    pipe.apply_next_tick();
    pipe.clear();
    pipe.apply_next_tick(); // Clear takes effect
    assert(!pipe.can_pop()); // Cleared
    assert(pipe.can_push());

    // Test capacity limit: BufSize=2, max queue + input buffer = 3
    // Fill to capacity
    assert(pipe.push(100));
    pipe.apply_next_tick(); // queue:0, pop_buffer:100
    assert(pipe.can_push());
    assert(pipe.push(101));
    pipe.apply_next_tick(); // queue:1, pop_buffer:100
    assert(pipe.can_push());
    assert(pipe.push(102));
    pipe.apply_next_tick(); // queue:2, pop_buffer:100, full=true
    assert(!pipe.can_push()); // Full, cannot push

    // Pop to make space
    assert(pipe.pop(data)); assert(data == 100);
    pipe.apply_next_tick(); // queue:2, pop_buffer:false, full=false
    assert(pipe.can_push()); // Now can push
}

void test_pipe_buffered_multi_port_depth1() {
    PipeBufferedMultiPortDepth1<int, 2, 2, 2> pipe; // PushWidth=2, PopWidth=2, BufSize=2
    int data;

    // Initial state
    assert(pipe.can_push());
    assert(!pipe.can_pop());

    // Push two data in one tick
    assert(pipe.push(10));
    assert(pipe.push(20));
    assert(!pipe.can_push()); // PushWidth=2 reached
    pipe.apply_next_tick();
    assert(pipe.can_pop());

    // Pop two data
    assert(pipe.pop(data)); assert(data == 10);
    assert(pipe.pop(data)); assert(data == 20);
    assert(!pipe.can_pop()); // PopWidth=2 reached
    pipe.apply_next_tick();

    // Test capacity: BufSize=2, max elements=3
    // Fill to near capacity
    assert(pipe.push(30));
    pipe.apply_next_tick(); // pop_buffer:30
    assert(pipe.push(31));
    pipe.apply_next_tick(); // queue:1, pop_buffer:30
    assert(pipe.push(32)); // queue:1, pop_buffer:1, pending:0, 1+1+0+1=3 <=3
    assert(!pipe.push(33)); // Now queue:1, pop_buffer:1, pending:1, 1+1+1+1=4 >3, cannot push
    assert(!pipe.can_push());
    pipe.apply_next_tick(); // Process pending, queue:2, pop_buffer:1, pending:0
    assert(!pipe.can_push()); // Full

    // Pop to make space
    assert(pipe.pop(data)); assert(data == 30);
    pipe.apply_next_tick(); // queue:2, pop_buffer:0
    assert(pipe.can_push()); // Now can push
    assert(pipe.pop(data)); assert(data == 31);
}

void test_vul_pipe() {
    // Test various VulPipe instantiations to ensure correct type selection
    // These should compile without static_assert errors

    // Handshake=true, Depth=1, PushWidth=2, PopWidth=2, BufSize=2 -> PipeBufferedMultiPortDepth1
    VulPipe<int, 2, 2, 2, 1, true> pipe1;
    assert(pipe1.can_push());
    assert(pipe1.push(1));
    assert(pipe1.push(2));
    pipe1.apply_next_tick();
    int data;
    assert(pipe1.pop(data)); assert(data == 1);
    assert(pipe1.pop(data)); assert(data == 2);
    pipe1.apply_next_tick();

    // Handshake=true, Depth=1, PushWidth=1, PopWidth=1, BufSize=2 -> PipeBufferedDepth1
    VulPipe<int, 1, 1, 2, 1, true> pipe2;
    assert(pipe2.can_push());
    assert(pipe2.push(3));
    pipe2.apply_next_tick();
    assert(pipe2.pop(data)); assert(data == 3);
    pipe2.apply_next_tick();

    // Handshake=true, Depth=1, PushWidth=1, PopWidth=1, BufSize=0 -> PipeDepth1
    VulPipe<int, 1, 1, 0, 1, true> pipe3;
    assert(pipe3.can_push());
    assert(pipe3.push(4));
    pipe3.apply_next_tick();
    assert(pipe3.pop(data)); assert(data == 4);
    pipe3.apply_next_tick();

    // Handshake=false, BufSize=0, PushWidth=2, PopWidth=2, Depth=1 -> PipeNoBlockMultiPort
    VulPipe<int, 2, 2, 0, 1, false> pipe4;
    assert(pipe4.can_push());
    assert(pipe4.push(5));
    assert(pipe4.push(6));
    pipe4.apply_next_tick();
    assert(pipe4.pop(data)); assert(data == 5);
    assert(pipe4.pop(data)); assert(data == 6);
    pipe4.apply_next_tick();

    // Handshake=false, BufSize=0, PushWidth=1, PopWidth=1, Depth=2 -> PipeNoBlock
    VulPipe<int, 1, 1, 0, 2, false> pipe5;
    assert(pipe5.can_push());
    assert(pipe5.push(7));
    pipe5.apply_next_tick();
    pipe5.apply_next_tick(); // Depth=2, need 2 ticks
    assert(pipe5.pop(data)); assert(data == 7);
    pipe5.apply_next_tick();

    // Handshake=false, BufSize=0, PushWidth=1, PopWidth=1, Depth=1 -> PipeNoBlockDepth1
    VulPipe<int, 1, 1, 0, 1, false> pipe6;
    assert(pipe6.can_push());
    assert(pipe6.push(8));
    pipe6.apply_next_tick();
    assert(pipe6.pop(data)); assert(data == 8);
    pipe6.apply_next_tick();
}

int main() {
    test_basic();
    test_multiple_pushes();
    test_clear();
    test_clear_after_push();
    test_multi_port();
    test_pipe_depth1();
    test_pipe_buffered_depth1();
    test_pipe_buffered_multi_port_depth1();
    test_vul_pipe();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}