#include "queue.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <random>

namespace {

template <typename T, uint32_t Depth>
struct RefQueue {
    std::deque<T> q;
    bool enq_pending = false;
    T enq_value{};
    bool deq_pending = false;
    bool clr_pending = false;

    bool enqready() const {
        return q.size() < Depth;
    }

    bool deqvalid() const {
        return !q.empty();
    }

    const T& front() const {
        return q.front();
    }

    void enqnext(const T& value) {
        assert(enqready());
        enq_pending = true;
        enq_value = value;
    }

    void deqnext() {
        assert(deqvalid());
        deq_pending = true;
    }

    void clrnext() {
        clr_pending = true;
    }

    void apply_next_tick() {
        const bool clr_this_tick = clr_pending;
        if (clr_pending) {
            q.clear();
            clr_pending = false;
        }
        if (deq_pending && !q.empty()) {
            q.pop_front();
        }
        if (!clr_this_tick && enq_pending && q.size() < Depth) {
            q.push_back(enq_value);
        }
        enq_pending = false;
        deq_pending = false;
    }
};

template <typename T, uint32_t Depth, uint32_t EnqWidth, uint32_t DeqWidth>
struct RefQueueMP {
    std::deque<T> q;
    std::array<T, EnqWidth> enq_values{};
    uint32_t enq_pending_num = 0;
    uint32_t deq_pending_num = 0;
    bool clr_pending = false;

    uint32_t enqreqdy() const {
        const uint32_t free_slots = Depth - static_cast<uint32_t>(q.size());
        return std::min<uint32_t>(free_slots, EnqWidth);
    }

    uint32_t deqvalid() const {
        return std::min<uint32_t>(static_cast<uint32_t>(q.size()), DeqWidth);
    }

    std::array<T, DeqWidth> front() const {
        std::array<T, DeqWidth> out{};
        const uint32_t valid = deqvalid();
        for (uint32_t i = 0; i < valid; ++i) {
            out[i] = q[i];
        }
        return out;
    }

    void enqnext(const std::array<T, EnqWidth>& values, uint32_t num) {
        const uint32_t req = std::min<uint32_t>(num, EnqWidth);
        assert(req <= enqreqdy());
        for (uint32_t i = 0; i < req; ++i) {
            enq_values[i] = values[i];
        }
        enq_pending_num = req;
    }

    void deqnext(uint32_t num) {
        const uint32_t req = std::min<uint32_t>(num, DeqWidth);
        assert(req <= deqvalid());
        deq_pending_num = req;
    }

    void clrnext() {
        clr_pending = true;
    }

    void apply_next_tick() {
        const bool clr_this_tick = clr_pending;
        if (clr_pending) {
            q.clear();
            clr_pending = false;
        }

        uint32_t pop_num = std::min<uint32_t>(deq_pending_num, static_cast<uint32_t>(q.size()));
        while (pop_num > 0) {
            q.pop_front();
            --pop_num;
        }

        uint32_t push_num = std::min<uint32_t>(enq_pending_num, Depth - static_cast<uint32_t>(q.size()));
        if (clr_this_tick) {
            push_num = 0;
        }
        for (uint32_t i = 0; i < push_num; ++i) {
            q.push_back(enq_values[i]);
        }

        enq_pending_num = 0;
        deq_pending_num = 0;
    }
};

void stress_single_port() {
    std::mt19937 rng(0x12345678u);
    VulQueue<int, 4> dut;
    RefQueue<int, 4> ref;

    for (int tick = 0; tick < 20000; ++tick) {
        assert(dut.enqready() == ref.enqready());
        assert(dut.enqready() == ref.enqready());
        assert(dut.deqvalid() == ref.deqvalid());
        assert(dut.deqvalid() == ref.deqvalid());

        if (dut.deqvalid()) {
            assert(dut.front() == ref.front());
            assert(dut.front() == ref.front());
        }

        if ((rng() & 7u) == 0u) {
            dut.clrnext();
            ref.clrnext();
        }

        if ((rng() & 1u) == 0u && dut.enqready()) {
            const int value = static_cast<int>(rng());
            dut.enqnext(value);
            ref.enqnext(value);
        }

        if ((rng() & 1u) == 0u && dut.deqvalid()) {
            assert(dut.front() == ref.front());
            dut.deqnext();
            ref.deqnext();
        }

        assert(dut.enqready() == ref.enqready());
        assert(dut.deqvalid() == ref.deqvalid());
        dut.apply_next_tick();
        ref.apply_next_tick();

        if (dut.deqvalid()) {
            assert(ref.deqvalid());
            assert(dut.front() == ref.front());
        } else {
            assert(!ref.deqvalid());
        }
    }
}

void stress_multi_port() {
    std::mt19937 rng(0x87654321u);
    VulQueueMP<int, 8, 3, 2> dut;
    RefQueueMP<int, 8, 3, 2> ref;

    for (int tick = 0; tick < 20000; ++tick) {
        assert(dut.enqreqdy() == ref.enqreqdy());
        assert(dut.enqreqdy() == ref.enqreqdy());
        assert(dut.deqvalid() == ref.deqvalid());
        assert(dut.deqvalid() == ref.deqvalid());

        {
            const uint32_t valid = ref.deqvalid();
            const auto out_dut = dut.front();
            const auto out_ref = ref.front();
            for (uint32_t i = 0; i < valid; ++i) {
                assert(out_dut[i] == out_ref[i]);
            }
        }

        if ((rng() & 7u) == 0u) {
            dut.clrnext();
            ref.clrnext();
        }

        if ((rng() & 1u) == 0u) {
            std::array<int, 3> values = {
                static_cast<int>(rng()),
                static_cast<int>(rng()),
                static_cast<int>(rng())
            };
            const uint32_t rdy = ref.enqreqdy();
            const uint32_t req = rdy == 0 ? 0 : static_cast<uint32_t>(rng() % (rdy + 1));
            dut.enqnext(values, req);
            ref.enqnext(values, req);
        }

        if ((rng() & 1u) == 0u) {
            const uint32_t valid = ref.deqvalid();
            if (valid > 0) {
                const auto out_dut = dut.front();
                const auto out_ref = ref.front();
                for (uint32_t i = 0; i < valid; ++i) {
                    assert(out_dut[i] == out_ref[i]);
                }
                const uint32_t req = static_cast<uint32_t>(rng() % (valid + 1));
                dut.deqnext(req);
                ref.deqnext(req);
            }
        }

        assert(dut.enqreqdy() == ref.enqreqdy());
        assert(dut.deqvalid() == ref.deqvalid());
        dut.apply_next_tick();
        ref.apply_next_tick();

        const uint32_t valid = ref.deqvalid();
        assert(dut.deqvalid() == valid);
        const auto out_dut = dut.front();
        const auto out_ref = ref.front();
        for (uint32_t i = 0; i < valid; ++i) {
            assert(out_dut[i] == out_ref[i]);
        }
    }
}

} // namespace

int main() {
    stress_single_port();
    stress_multi_port();
    std::cout << "Queue stress tests passed!" << std::endl;
    return 0;
}
