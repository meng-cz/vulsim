#include "queue.hpp"

#include <cassert>
#include <iostream>

namespace {

void test_empty_and_basic_enq_deq() {
    VulQueue<int, 2> q;

    assert(!q.deqvalid());
    assert(q.enqready());

    assert(q.enqnext(10));
    assert(!q.deqvalid());
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqnext() == 10);
    assert(q.enqready());
    q.apply_next_tick();

    assert(!q.deqvalid());
    assert(q.enqready());
}

void test_full_condition() {
    VulQueue<int, 2> q;

    assert(q.enqnext(1));
    q.apply_next_tick();
    assert(q.enqnext(2));
    q.apply_next_tick();

    assert(!q.enqready());
    assert(!q.enqnext(3));
}

void test_fifo_order() {
    VulQueue<int, 3> q;

    assert(q.enqnext(11));
    q.apply_next_tick();
    assert(q.enqnext(22));
    q.apply_next_tick();
    assert(q.enqnext(33));
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqnext() == 11);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqnext() == 22);
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqnext() == 33);
    q.apply_next_tick();

    assert(!q.deqvalid());
}

void test_enq_and_deq_same_tick_apply_together() {
    VulQueue<int, 2> q;

    assert(q.enqnext(7));
    q.apply_next_tick();
    assert(q.enqnext(8));
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqnext() == 7);
    assert(!q.enqready());

    // This enqueue is blocked because queue is still full in this cycle.
    assert(!q.enqnext(9));
    q.apply_next_tick();

    assert(q.enqready());
    assert(q.enqnext(9));
    q.apply_next_tick();

    assert(q.deqvalid());
    assert(q.deqnext() == 8);
    q.apply_next_tick();
    assert(q.deqvalid());
    assert(q.deqnext() == 9);
    q.apply_next_tick();
}

void test_clrnext() {
    VulQueue<int, 4> q;

    assert(q.enqnext(100));
    q.apply_next_tick();
    assert(q.enqnext(200));
    q.apply_next_tick();
    assert(q.deqvalid());

    q.clrnext();
    // Clear takes effect only after apply_next_tick().
    assert(q.deqvalid());
    q.apply_next_tick();

    assert(!q.deqvalid());
    assert(q.enqready());
    assert(q.enqnext(300));
    q.apply_next_tick();
    assert(q.deqvalid());
    assert(q.deqnext() == 300);
}

void test_mp_basic_pack_enq_deq() {
    VulQueueMP<int, 8, 3, 2> q;

    assert(q.enqreqdy() == 3);
    assert(q.deqvalid() == 0);

    std::array<int, 3> in = {1, 2, 3};
    assert(q.enqnext(in, 3) == 3);
    assert(q.enqreqdy() == 0);
    q.apply_next_tick();

    assert(q.deqvalid() == 2);
    const auto &out0 = q.deqnext(2);
    assert(out0[0] == 1);
    assert(out0[1] == 2);
    assert(q.deqvalid() == 0);
    q.apply_next_tick();

    assert(q.deqvalid() == 1);
    const auto &out1 = q.deqnext(1);
    assert(out1[0] == 3);
    q.apply_next_tick();

    assert(q.deqvalid() == 0);
}

void test_mp_partial_accept_and_fifo() {
    VulQueueMP<int, 4, 3, 3> q;

    std::array<int, 3> in0 = {10, 11, 12};
    assert(q.enqnext(in0, 3) == 3);
    q.apply_next_tick();

    // Queue has only one free slot now.
    std::array<int, 3> in1 = {13, 14, 15};
    assert(q.enqreqdy() == 1);
    assert(q.enqnext(in1, 3) == 1);
    q.apply_next_tick();

    assert(q.deqvalid() == 3);
    const auto &out0 = q.deqnext(3);
    assert(out0[0] == 10);
    assert(out0[1] == 11);
    assert(out0[2] == 12);
    q.apply_next_tick();

    assert(q.deqvalid() == 1);
    const auto &out1 = q.deqnext(1);
    assert(out1[0] == 13);
    q.apply_next_tick();

    assert(q.deqvalid() == 0);
}

void test_mp_same_tick_deq_not_free_for_enq_and_clr() {
    VulQueueMP<int, 4, 2, 2> q;

    std::array<int, 2> in0 = {1, 2};
    std::array<int, 2> in1 = {3, 4};
    assert(q.enqnext(in0, 2) == 2);
    q.apply_next_tick();
    assert(q.enqnext(in1, 2) == 2);
    q.apply_next_tick();

    // Full queue, dequeue in this tick does not free enqueue space immediately.
    assert(q.enqreqdy() == 0);
    const auto &out0 = q.deqnext(2);
    assert(out0[0] == 1);
    assert(out0[1] == 2);
    std::array<int, 2> in2 = {5, 6};
    assert(q.enqnext(in2, 2) == 0);
    q.apply_next_tick();

    assert(q.deqvalid() == 2);
    const auto &out1 = q.deqnext(2);
    assert(out1[0] == 3);
    assert(out1[1] == 4);

    // Clear takes effect in next apply.
    q.clrnext();
    assert(q.deqvalid() == 0);
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
    test_mp_basic_pack_enq_deq();
    test_mp_partial_accept_and_fifo();
    test_mp_same_tick_deq_not_free_for_enq_and_clr();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
