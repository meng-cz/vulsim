#include "queue.hpp"

#include <cassert>
#include <iostream>

namespace {

void test_empty_and_basic_enq_deq() {
    VulQueue<int, 2> q;

    assert(!q.deqvalid());
    assert(q.enqready());

    q.enqnext(10);
    assert(!q.deqvalid());
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 10);
    q.deqnext();
    assert(q.enqready());
    q.apply_next_tick();

    assert(!q.deqvalid());
    assert(q.enqready());
}

void test_full_condition() {
    VulQueue<int, 2> q;

    q.enqnext(1);
    q.apply_next_tick();
    q.enqnext(2);
    q.apply_next_tick();

    assert(!q.enqready());
}

void test_fifo_order() {
    VulQueue<int, 3> q;

    q.enqnext(11);
    q.apply_next_tick();
    q.enqnext(22);
    q.apply_next_tick();
    q.enqnext(33);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 11);
    q.deqnext();
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 22);
    q.deqnext();
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 33);
    q.deqnext();
    q.apply_next_tick();

    assert(!q.deqvalid());
}

void test_enq_and_deq_same_tick_apply_together() {
    VulQueue<int, 2> q;

    q.enqnext(7);
    q.apply_next_tick();
    q.enqnext(8);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 7);
    q.deqnext();
    assert(!q.enqready());

    // This enqueue is blocked because queue is still full in this cycle.
    q.apply_next_tick();

    assert(q.enqready());
    q.enqnext(9);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 8);
    q.deqnext();
    q.apply_next_tick();
    assert(q.deqvalid());
    assert(q.front() == 9);
    q.deqnext();
    q.apply_next_tick();
}

void test_clrnext() {
    VulQueue<int, 4> q;

    q.enqnext(100);
    q.apply_next_tick();
    q.enqnext(200);
    q.apply_next_tick();
    assert(q.deqvalid());

    q.clrnext();
    // Clear takes effect only after apply_next_tick().
    assert(q.deqvalid());
    q.apply_next_tick();

    assert(!q.deqvalid());
    assert(q.enqready());
    q.enqnext(300);
    q.apply_next_tick();
    assert(q.deqvalid());
    assert(q.front() == 300);
    q.deqnext();
}

void test_clrnext_drops_same_cycle_enq() {
    VulQueue<int, 4> q;

    q.enqnext(100);
    q.apply_next_tick();
    assert(q.deqvalid());

    q.clrnext();
    q.enqnext(200);
    q.apply_next_tick();

    assert(!q.deqvalid());
    assert(q.enqready());
}

void test_front_does_not_schedule_deq() {
    VulQueue<int, 2> q;

    assert(q.enqready());
    assert(q.enqready());
    q.enqnext(10);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqvalid());
    assert(q.front() == 10);
    assert(q.front() == 10);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.front() == 10);
    q.deqnext();
    q.apply_next_tick();
    assert(!q.deqvalid());
}

void test_mp_basic_pack_enq_deq() {
    VulQueueMP<int, 8, 3, 2> q;

    assert(q.enqreqdy() == 3);
    assert(q.enqreqdy() == 3);
    assert(q.deqvalid() == 0);
    assert(q.deqvalid() == 0);

    std::array<int, 3> in = {1, 2, 3};
    q.enqnext(in, 3);
    assert(q.enqreqdy() == 3);
    q.apply_next_tick();

    assert(q.deqvalid() == 2);
    assert(q.deqvalid() == 2);
    const auto &out0 = q.front(2);
    assert(out0[0] == 1);
    assert(out0[1] == 2);
    const auto &out0_again = q.front(2);
    assert(out0_again[0] == 1);
    assert(out0_again[1] == 2);
    q.deqnext(2);
    assert(q.deqvalid() == 2);
    q.apply_next_tick();

    assert(q.deqvalid() == 1);
    const auto &out1 = q.front(1);
    assert(out1[0] == 3);
    q.deqnext(1);
    q.apply_next_tick();

    assert(q.deqvalid() == 0);
}

void test_mp_partial_accept_and_fifo() {
    VulQueueMP<int, 4, 3, 3> q;

    std::array<int, 3> in0 = {10, 11, 12};
    q.enqnext(in0, 3);
    q.apply_next_tick();

    // Queue has only one free slot now.
    std::array<int, 3> in1 = {13, 14, 15};
    assert(q.enqreqdy() == 1);
    q.enqnext(in1, 1);
    q.apply_next_tick();

    assert(q.deqvalid() == 3);
    const auto &out0 = q.front(3);
    assert(out0[0] == 10);
    assert(out0[1] == 11);
    assert(out0[2] == 12);
    q.deqnext(3);
    q.apply_next_tick();

    assert(q.deqvalid() == 1);
    const auto &out1 = q.front(1);
    assert(out1[0] == 13);
    q.deqnext(1);
    q.apply_next_tick();

    assert(q.deqvalid() == 0);
}

void test_mp_same_tick_deq_not_free_for_enq_and_clr() {
    VulQueueMP<int, 4, 2, 2> q;

    std::array<int, 2> in0 = {1, 2};
    std::array<int, 2> in1 = {3, 4};
    q.enqnext(in0, 2);
    q.apply_next_tick();
    q.enqnext(in1, 2);
    q.apply_next_tick();

    // Full queue, dequeue in this tick does not free enqueue space immediately.
    assert(q.enqreqdy() == 0);
    const auto &out0 = q.front(2);
    assert(out0[0] == 1);
    assert(out0[1] == 2);
    q.deqnext(2);
    std::array<int, 2> in2 = {5, 6};
    q.apply_next_tick();

    assert(q.deqvalid() == 2);
    const auto &out1 = q.front(2);
    assert(out1[0] == 3);
    assert(out1[1] == 4);
    q.deqnext(2);

    // Clear takes effect in next apply.
    q.clrnext();
    assert(q.deqvalid() == 2);
    q.apply_next_tick();
    assert(q.deqvalid() == 0);
    assert(q.enqreqdy() == 2);
}

void test_mp_clrnext_drops_same_cycle_enq() {
    VulQueueMP<int, 4, 2, 2> q;

    std::array<int, 2> in0 = {1, 2};
    std::array<int, 2> in1 = {3, 4};

    q.enqnext(in0, 2);
    q.apply_next_tick();
    assert(q.deqvalid() == 2);

    q.clrnext();
    q.enqnext(in1, 2);
    q.apply_next_tick();

    assert(q.deqvalid() == 0);
    assert(q.enqreqdy() == 2);
}

} // namespace

int main() {
    test_empty_and_basic_enq_deq();
    test_full_condition();
    test_fifo_order();
    test_enq_and_deq_same_tick_apply_together();
    test_clrnext();
    test_clrnext_drops_same_cycle_enq();
    test_front_does_not_schedule_deq();
    test_mp_basic_pack_enq_deq();
    test_mp_partial_accept_and_fifo();
    test_mp_same_tick_deq_not_free_for_enq_and_clr();
    test_mp_clrnext_drops_same_cycle_enq();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
