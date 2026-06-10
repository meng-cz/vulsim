
#include "ram.hpp"

#include <cassert>
#include <iostream>

namespace {

template <uint32_t W>
void expect_eq(const Int<W>& got, uint64_t expected) {
    assert(got == Int<W>(expected));
}

void test_single_cycle_read_latency() {
    VulBRAM<Int<16>, 16, 1, 1> ram;

    ram.readreq<0>(Int<4>(3));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);

    ram.write<0>(Int<4>(3), Int<16>(0x55aa));
    ram.readreq<0>(Int<4>(3));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);

    ram.readreq<0>(Int<4>(3));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x55aa);

    // Change read address; result appears after the next tick.
    ram.readreq<0>(Int<4>(2));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);
}

void test_1rw_read_write_same_address_returns_old_value() {
    VulBRAM1RW<Int<16>, 16> ram;

    ram.req(Int<4>(4), Int<16>(0x1357), true);
    ram.apply_next_tick();
    expect_eq(ram.readdata(), 0);

    ram.req(Int<4>(4), Int<16>(0xabcd), true);
    ram.apply_next_tick();
    expect_eq(ram.readdata(), 0x1357);

    ram.req(Int<4>(4), Int<16>(0), false);
    ram.apply_next_tick();
    expect_eq(ram.readdata(), 0xabcd);
}

void test_multi_read_ports() {
    VulBRAM<Int<8>, 8, 2, 1> ram;

    ram.write<0>(Int<3>(1), Int<8>(0x12));
    ram.readreq<0>(Int<3>(1));
    ram.readreq<1>(Int<3>(0));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x00);
    expect_eq(ram.readdata<1>(), 0x00);

    ram.readreq<0>(Int<3>(1));
    ram.readreq<1>(Int<3>(0));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x12);
    expect_eq(ram.readdata<1>(), 0x00);

    ram.write<0>(Int<3>(0), Int<8>(0x34));
    ram.readreq<0>(Int<3>(1));
    ram.readreq<1>(Int<3>(0));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x12);
    expect_eq(ram.readdata<1>(), 0x00);

    ram.readreq<0>(Int<3>(1));
    ram.readreq<1>(Int<3>(0));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x12);
    expect_eq(ram.readdata<1>(), 0x34);
}

void test_read_write_same_address_returns_old_value() {
    VulBRAM<Int<32>, 32, 2, 1> ram;

    ram.write<0>(Int<5>(7), Int<32>(0xdeadbeefULL));
    ram.readreq<0>(Int<5>(7));
    ram.readreq<1>(Int<5>(7));
    ram.apply_next_tick();

    // Same-cycle read/write to same address returns the pre-write value.
    expect_eq(ram.readdata<0>(), 0);
    expect_eq(ram.readdata<1>(), 0);

    ram.readreq<0>(Int<5>(7));
    ram.readreq<1>(Int<5>(7));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0xdeadbeefULL);
    expect_eq(ram.readdata<1>(), 0xdeadbeefULL);
}

void test_multi_write_port_priority() {
    VulBRAM<Int<16>, 16, 1, 3> ram;

    // All writes target the same address in one cycle.
    ram.write<0>(Int<4>(9), Int<16>(0x0011));
    ram.write<1>(Int<4>(9), Int<16>(0x0022));
    ram.write<2>(Int<4>(9), Int<16>(0x0033));
    ram.readreq<0>(Int<4>(9));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);

    // Larger port index has higher priority.
    ram.readreq<0>(Int<4>(9));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0x0033);
}

void test_write_enable_cleared_between_ticks() {
    VulBRAM<Int<8>, 8, 1, 2> ram;

    ram.write<1>(Int<3>(5), Int<8>(0xa5));
    ram.readreq<0>(Int<3>(5));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0);

    ram.readreq<0>(Int<3>(5));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0xa5);

    // No new write this cycle; previous write-enable bits must be cleared.
    ram.readreq<0>(Int<3>(5));
    ram.apply_next_tick();
    expect_eq(ram.readdata<0>(), 0xa5);
}

} // namespace

int main() {
    test_single_cycle_read_latency();
    test_1rw_read_write_same_address_returns_old_value();
    test_multi_read_ports();
    test_read_write_same_address_returns_old_value();
    test_multi_write_port_priority();
    test_write_enable_cleared_between_ticks();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
