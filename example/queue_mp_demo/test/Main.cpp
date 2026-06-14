#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../Top.hpp");
PROJECT("..");

REQUEST(push, ARG(U32x3) values, ARG(uint32_t) num);
REQUEST(pop, ARG(uint32_t) num, RESP(uint32_t) valid_before, RESP(U32x2) values);
REQUEST(pop_push,
        ARG(uint32_t) pop_num,
        ARG(U32x3) values,
        ARG(uint32_t) push_num,
        RESP(uint32_t) ready_before,
        RESP(uint32_t) valid_before,
        RESP(uint32_t) ready_after,
        RESP(U32x2) popped);
REQUEST(clear_push, ARG(U32x3) values, ARG(uint32_t) num);
QUERY(status, QueueMPStatus);

SIMULATION() {
    auto fail = [&](const char *msg, uint64_t got, uint64_t expected) {
        std::printf("queue_mp demo failed: %s got=%llu expected=%llu\n",
                    msg,
                    static_cast<unsigned long long>(got),
                    static_cast<unsigned long long>(expected));
        std::exit(1);
    };
    auto check = [&](bool cond, const char *msg, uint64_t got, uint64_t expected) {
        if (!cond) {
            fail(msg, got, expected);
        }
    };

    QueueMPStatus st = status();
    check(st.enq_rdy == 3U, "initial enq_rdy", st.enq_rdy, 3);
    check(st.deq_valid == 0U, "initial deq_valid", st.deq_valid, 0);
    sim_execute();
    sim_commit();

    U32x3 in0{1, 2, 3};
    push(in0, 3U);
    sim_execute();
    sim_commit();

    st = status();
    check(st.enq_rdy == 1U, "after push enq_rdy", st.enq_rdy, 1);
    check(st.deq_valid == 2U, "after push deq_valid", st.deq_valid, 2);
    sim_execute();
    sim_commit();

    uint32_t valid_before = 0;
    U32x2 out{};
    pop(2U, valid_before, out);
    check(valid_before == 2U, "pop valid_before", valid_before, 2);
    check(out[0] == 1U, "pop out0", out[0], 1);
    check(out[1] == 2U, "pop out1", out[1], 2);
    sim_execute();
    sim_commit();

    st = status();
    check(st.enq_rdy == 3U, "after pop enq_rdy", st.enq_rdy, 3);
    check(st.deq_valid == 1U, "after pop deq_valid", st.deq_valid, 1);
    sim_execute();
    sim_commit();

    U32x3 in1{10, 11, 12};
    push(in1, 3U);
    sim_execute();
    sim_commit();

    st = status();
    check(st.enq_rdy == 0U, "queue full enq_rdy", st.enq_rdy, 0);
    check(st.deq_valid == 2U, "queue full deq_valid", st.deq_valid, 2);
    sim_execute();
    sim_commit();

    U32x3 in2{20, 21, 22};
    uint32_t ready_before = 0;
    uint32_t ready_after = 0;
    pop_push(2U, in2, 0U, ready_before, valid_before, ready_after, out);
    check(ready_before == 0U, "pop_push ready_before", ready_before, 0);
    check(ready_after == 0U, "pop_push ready_after", ready_after, 0);
    check(valid_before == 2U, "pop_push valid_before", valid_before, 2);
    check(out[0] == 3U, "pop_push out0", out[0], 3);
    check(out[1] == 10U, "pop_push out1", out[1], 10);
    sim_execute();
    sim_commit();

    st = status();
    check(st.enq_rdy == 2U, "after pop_push enq_rdy", st.enq_rdy, 2);
    check(st.deq_valid == 2U, "after pop_push deq_valid", st.deq_valid, 2);
    sim_execute();
    sim_commit();

    U32x3 in3{30, 31, 32};
    clear_push(in3, 2U);
    sim_execute();
    sim_commit();

    st = status();
    check(st.enq_rdy == 3U, "after clear_push enq_rdy", st.enq_rdy, 3);
    check(st.deq_valid == 0U, "after clear_push deq_valid", st.deq_valid, 0);
    sim_execute();
    sim_commit();

    U32x3 in4{100, 101, 102};
    push(in4, 3U);
    sim_execute();
    sim_commit();

    U32x3 in5{200, 201, 202};
    st = status();
    check(st.enq_rdy == 1U, "partial accept ready", st.enq_rdy, 1);
    push(in5, 1U);
    sim_execute();
    sim_commit();

    st = status();
    check(st.deq_valid == 2U, "after partial fill deq_valid", st.deq_valid, 2);
    pop(2U, valid_before, out);
    check(valid_before == 2U, "final pop valid_before", valid_before, 2);
    check(out[0] == 100U, "final pop out0", out[0], 100);
    check(out[1] == 101U, "final pop out1", out[1], 101);
    sim_execute();
    sim_commit();

    std::printf("queue_mp demo passed\n");
}
