#include "storage.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

template <typename Reg, typename Value>
void force_reset(Reg &reg, const Value &value) {
    reg._set_reset_value(value);
    reg._reset();
}

void test_single_register_basic() {
    VulRegister<int> reg;
    force_reset(reg, 1);
    reg.setnext(5);
    reg.apply_next_tick();
    assert(reg.get() == 5);
}

void test_multiport_register_priority() {
    VulRegister<int, 2> reg;
    force_reset(reg, 0);
    reg.setnext<1>(7);
    reg.setnext<0>(9);
    reg.apply_next_tick();
    assert(reg.get() == 9);
}

void test_register_holdnext_and_resetnext_priority() {
    VulRegister<int, 2> reg;
    force_reset(reg, 4);
    reg._set_reset_value(12);

    reg.setnext<0>(8);
    reg.holdnext();
    reg.apply_next_tick();
    assert(reg.get() == 4);

    reg.setnext<1>(9);
    reg.holdnext();
    reg.resetnext();
    reg.apply_next_tick();
    assert(reg.get() == 12);

    reg.holdnext();
    reg.setnext<0>(15);
    reg.apply_next_tick();
    assert(reg.get() == 12);
}

void test_register_array_basic() {
    VulRegisterArray<int, 32, 2> reg_array;
    force_reset(reg_array, 0);
    reg_array.setnext<1>(7, 11);
    reg_array.setnext<0>(7, 13);
    reg_array.setnext<0>(3, 5);
    reg_array.apply_next_tick();
    assert(reg_array[7] == 13);
    assert(reg_array[3] == 5);
}

void test_register_array_holdnext_and_resetnext() {
    VulRegisterArray<int, 32, 2> reg_array;
    std::array<int, 32> reset_values{};
    for (uint32_t i = 0; i < reset_values.size(); ++i) {
        reset_values[i] = 100 + static_cast<int>(i);
    }
    force_reset(reg_array, reset_values);

    reg_array.setnext<0>(7, 1);
    reg_array.holdnext(7);
    reg_array.setnext<0>(8, 2);
    reg_array.apply_next_tick();
    assert(reg_array[7] == 107);
    assert(reg_array[8] == 2);

    reg_array.setnext<0>(7, 3);
    reg_array.resetnext(7);
    reg_array.setnext<0>(8, 4);
    reg_array.holdnext();
    reg_array.apply_next_tick();
    assert(reg_array[7] == 107);
    assert(reg_array[8] == 2);

    reg_array.setnext<0>(8, 5);
    reg_array.resetnext();
    reg_array.apply_next_tick();
    assert(reg_array[7] == 107);
    assert(reg_array[8] == 108);
}

struct PendingWrite {
    uint32_t port;
    uint32_t index;
    int value;
};

void apply_writes(
    vulstorage::VulRegisterArrayFullImpl<int, 32, 3> &full,
    vulstorage::VulRegisterArrayDirtyImpl<int, 32, 3> &dirty,
    const std::vector<PendingWrite> &writes) {
    for (const auto &write : writes) {
        switch (write.port) {
            case 0:
                full.setnext<0>(write.index, write.value);
                dirty.setnext<0>(write.index, write.value);
                break;
            case 1:
                full.setnext<1>(write.index, write.value);
                dirty.setnext<1>(write.index, write.value);
                break;
            case 2:
                full.setnext<2>(write.index, write.value);
                dirty.setnext<2>(write.index, write.value);
                break;
            default:
                assert(false);
        }
    }
}

void assert_arrays_equal(
    const vulstorage::VulRegisterArrayFullImpl<int, 32, 3> &full,
    const vulstorage::VulRegisterArrayDirtyImpl<int, 32, 3> &dirty) {
    for (uint32_t index = 0; index < 32; ++index) {
        assert(full[index] == dirty[index]);
    }
}

void test_register_array_impl_equivalence() {
    vulstorage::VulRegisterArrayFullImpl<int, 32, 3> full;
    vulstorage::VulRegisterArrayDirtyImpl<int, 32, 3> dirty;

    force_reset(full, 0);
    force_reset(dirty, 0);
    assert_arrays_equal(full, dirty);

    std::mt19937 rng(20260610u);
    std::uniform_int_distribution<uint32_t> index_dist(0, 31);
    std::uniform_int_distribution<int> value_dist(-1000, 1000);
    std::bernoulli_distribution write_dist(0.55);
    std::bernoulli_distribution reset_dist(0.08);
    std::bernoulli_distribution array_reset_dist(0.35);

    for (uint32_t cycle = 0; cycle < 400; ++cycle) {
        if (reset_dist(rng)) {
            if (array_reset_dist(rng)) {
                std::array<int, 32> values{};
                for (auto &value : values) {
                    value = value_dist(rng);
                }
                force_reset(full, values);
                force_reset(dirty, values);
            } else {
                const int value = value_dist(rng);
                force_reset(full, value);
                force_reset(dirty, value);
            }
            assert_arrays_equal(full, dirty);
        }

        std::vector<PendingWrite> writes;
        for (uint32_t port = 0; port < 3; ++port) {
            std::array<uint8_t, 32> used_indices{};
            for (uint32_t attempt = 0; attempt < 18; ++attempt) {
                if (!write_dist(rng)) {
                    continue;
                }
                const uint32_t index = index_dist(rng);
                if (used_indices[index] != 0) {
                    continue;
                }
                used_indices[index] = 1;
                writes.push_back(PendingWrite{port, index, value_dist(rng)});
            }
        }

        apply_writes(full, dirty, writes);
        full.apply_next_tick();
        dirty.apply_next_tick();
        assert_arrays_equal(full, dirty);
    }
}

} // namespace

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "legal";

    if (mode == "legal") {
        test_single_register_basic();
        test_multiport_register_priority();
        test_register_holdnext_and_resetnext_priority();
        test_register_array_basic();
        test_register_array_holdnext_and_resetnext();
        test_register_array_impl_equivalence();
        std::cout << "Storage write contract tests passed!" << std::endl;
        return 0;
    }

    if (mode == "single_dup") {
        VulRegister<int> reg;
        reg.setnext(1);
        reg.setnext(2);
        return 0;
    }

    if (mode == "multi_dup_same_port") {
        VulRegister<int, 2> reg;
        reg.setnext<1>(3);
        reg.setnext<1>(4);
        return 0;
    }

    if (mode == "array_dup_same_cell_same_port") {
        VulRegisterArray<int, 32, 2> reg_array;
        reg_array.setnext<0>(9, 1);
        reg_array.setnext<0>(9, 2);
        return 0;
    }

    return 2;
}
