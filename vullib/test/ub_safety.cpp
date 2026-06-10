#include "ram.hpp"
#include "storage.hpp"
#include "uint.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace {

void test_uint_64bit_mask_paths() {
    Int<64> all_ones = ~Int<64>(0);
    assert(all_ones.reduce_and());
    assert(!all_ones.reduce_xor());

    auto plus_one = all_ones + uint64_t(1);
    static_assert(std::is_same_v<decltype(plus_one), Int<65>>);
    assert(plus_one(63, 0).template operator Int<64>() == Int<64>(0));
    assert(static_cast<bool>(plus_one(64)));

    auto minus_one = Int<64>(0) - uint64_t(1);
    assert(minus_one == Int<64>(~uint64_t(0)));
}

void test_bram_default_zero_init() {
    VulBRAM<int, 4, 1, 1> ram;
    ram.readreq<0>(Int<2>(0));
    ram.apply_next_tick();
    assert(ram.readdata<0>() == 0);

    VulBRAM1RW<int, 4> ram1rw;
    ram1rw.apply_next_tick();
    assert(ram1rw.readdata() == 0);
}

void test_storage_default_zero_init() {
    VulStorageNext<int, 2> reg;
    reg.apply_next_tick();
    assert(reg.get() == 0);

    VulStorageNextArray<int, 32, 2> array_reg;
    array_reg.apply_next_tick();
    for (uint32_t i = 0; i < 32; ++i) {
        assert(array_reg[i] == 0);
    }
}

} // namespace

int main() {
    test_uint_64bit_mask_paths();
    test_bram_default_zero_init();
    test_storage_default_zero_init();

    std::cout << "UB safety tests passed!" << std::endl;
    return 0;
}
