
#include "ram.hpp"

#include <cassert>
#include <iostream>

namespace {

template <uint32_t W>
void expect_eq(const UInt<W>& got, uint64_t expected) {
    assert(got == UInt<W>(expected));
}

void test_single_cycle_read_latency() {
    VulBRAM<16, 4, 1, 1> ram;

    ram.readreq<0>(UInt<4>(3));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);

    ram.write<0>(UInt<4>(3), UInt<16>(0x55aa));
    ram.readreq<0>(UInt<4>(3));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x55aa);

    // Change read address; result appears after the next tick.
    ram.readreq<0>(UInt<4>(2));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);
}

void test_multi_read_ports() {
    VulBRAM<8, 3, 2, 1> ram;

    ram.write<0>(UInt<3>(1), UInt<8>(0x12));
    ram.readreq<0>(UInt<3>(1));
    ram.readreq<1>(UInt<3>(0));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x12);
    expect_eq(ram.readdata<1>(), 0x00);

    ram.write<0>(UInt<3>(0), UInt<8>(0x34));
    ram.readreq<0>(UInt<3>(1));
    ram.readreq<1>(UInt<3>(0));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x12);
    expect_eq(ram.readdata<1>(), 0x34);
}

void test_read_write_same_address_write_first() {
    VulBRAM<32, 5, 2, 1> ram;

    ram.write<0>(UInt<5>(7), UInt<32>(0xdeadbeefULL));
    ram.readreq<0>(UInt<5>(7));
    ram.readreq<1>(UInt<5>(7));
    ram.apply_next_tick();

    // Same-cycle read/write to same address must return post-write value.
    expect_eq(ram.readdata<0>(), 0xdeadbeefULL);
    expect_eq(ram.readdata<1>(), 0xdeadbeefULL);
}

void test_multi_write_port_priority() {
    VulBRAM<16, 4, 1, 3> ram;

    // All writes target the same address in one cycle.
    ram.write<0>(UInt<4>(9), UInt<16>(0x0011));
    ram.write<1>(UInt<4>(9), UInt<16>(0x0022));
    ram.write<2>(UInt<4>(9), UInt<16>(0x0033));
    ram.readreq<0>(UInt<4>(9));
    ram.apply_next_tick();

    // Larger port index has higher priority.
    expect_eq(ram.readdata<0>(), 0x0033);
}

void test_write_enable_cleared_between_ticks() {
    VulBRAM<8, 3, 1, 2> ram;

    ram.write<1>(UInt<3>(5), UInt<8>(0xa5));
    ram.readreq<0>(UInt<3>(5));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0xa5);

    // No new write this cycle; previous write-enable bits must be cleared.
    ram.readreq<0>(UInt<3>(5));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0xa5);
}

} // namespace

int main() {
    test_single_cycle_read_latency();
    test_multi_read_ports();
    test_read_write_same_address_write_first();
    test_multi_write_port_priority();
    test_write_enable_cleared_between_ticks();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
