#include "ram.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

template <uint32_t W>
uint64_t as_u64(const Int<W> &value) {
    return value.template to<uint64_t>();
}

[[noreturn]] void fail(const char *message) {
    std::cerr << message << std::endl;
    std::abort();
}

template <typename RamT>
uint64_t bram_read_port(const RamT &ram, uint32_t port) {
    switch (port) {
        case 0: return as_u64(ram.template readdata<0>());
        case 1: return as_u64(ram.template readdata<1>());
        case 2: return as_u64(ram.template readdata<2>());
        default: fail("invalid BRAM read port");
    }
}

void test_bram_randomized_model() {
    constexpr uint64_t kSize = 6;
    constexpr uint32_t kReadPorts = 3;
    constexpr uint32_t kWritePorts = 2;
    constexpr uint32_t kCycles = 4000;

    VulBRAM<Int<16>, kSize, kReadPorts, kWritePorts> ram;
    std::array<uint16_t, kSize> model_memory{};
    std::array<bool, kReadPorts> expect_valid{};
    std::array<uint64_t, kReadPorts> expect_value{};

    std::mt19937_64 rng(0x1234abcddcba4321ULL);

    for (uint32_t cycle = 0; cycle < kCycles; ++cycle) {
        for (uint32_t port = 0; port < kReadPorts; ++port) {
            if (expect_valid[port]) {
                if (bram_read_port(ram, port) != expect_value[port]) {
                    fail("VulBRAM randomized model mismatch");
                }
            }
        }

        std::array<bool, kReadPorts> next_expect_valid{};
        std::array<uint64_t, kReadPorts> next_expect_value{};
        std::array<bool, kWritePorts> write_valid{};
        std::array<uint64_t, kWritePorts> write_addr{};
        std::array<uint16_t, kWritePorts> write_data{};

        for (uint32_t port = 0; port < kReadPorts; ++port) {
            if ((rng() & 1ULL) != 0) {
                const uint64_t addr = rng() % kSize;
                if constexpr (kReadPorts > 0) {
                    if (port == 0) ram.readreq<0>(Int<3>(addr));
                    if (port == 1) ram.readreq<1>(Int<3>(addr));
                    if (port == 2) ram.readreq<2>(Int<3>(addr));
                }
                next_expect_valid[port] = true;
                next_expect_value[port] = model_memory[addr];
            }
        }

        for (uint32_t port = 0; port < kWritePorts; ++port) {
            if ((rng() % 3ULL) == 0) {
                write_valid[port] = true;
                write_addr[port] = rng() % kSize;
                write_data[port] = static_cast<uint16_t>(rng());
                if constexpr (kWritePorts > 0) {
                    if (port == 0) ram.write<0>(Int<3>(write_addr[port]), Int<16>(write_data[port]));
                    if (port == 1) ram.write<1>(Int<3>(write_addr[port]), Int<16>(write_data[port]));
                }
            }
        }

        ram.apply_next_tick();

        for (uint32_t port = 0; port < kWritePorts; ++port) {
            if (write_valid[port]) {
                model_memory[write_addr[port]] = write_data[port];
            }
        }

        expect_valid = next_expect_valid;
        expect_value = next_expect_value;
    }

    for (uint32_t port = 0; port < kReadPorts; ++port) {
        if (expect_valid[port]) {
            if (bram_read_port(ram, port) != expect_value[port]) {
                fail("VulBRAM final read mismatch");
            }
        }
    }
}

void test_bram1rw_randomized_model() {
    constexpr uint64_t kSize = 5;
    constexpr uint32_t kCycles = 4000;

    VulBRAM1RW<Int<16>, kSize> ram;
    std::array<uint16_t, kSize> model_memory{};
    bool expect_valid = false;
    uint16_t expect_value = 0;

    std::mt19937_64 rng(0x55aa12345678ULL);

    for (uint32_t cycle = 0; cycle < kCycles; ++cycle) {
        if (expect_valid) {
            if (as_u64(ram.readdata()) != expect_value) {
                fail("VulBRAM1RW randomized model mismatch");
            }
        }

        const uint32_t op = static_cast<uint32_t>(rng() % 3ULL);
        const uint64_t addr = rng() % kSize;
        const uint16_t data = static_cast<uint16_t>(rng());

        if (op == 0) {
            ram.req(Int<3>(addr), Int<16>(0), false);
        } else if (op == 1) {
            ram.req(Int<3>(addr), Int<16>(data), true);
        }

        ram.apply_next_tick();

        if (op == 0) {
            expect_valid = true;
            expect_value = model_memory[addr];
        } else {
            expect_valid = false;
            if (op == 1) {
                model_memory[addr] = data;
            }
        }
    }

    if (expect_valid) {
        if (as_u64(ram.readdata()) != expect_value) {
            fail("VulBRAM1RW final read mismatch");
        }
    }
}

void test_rom_randomized_model() {
    constexpr uint64_t kSize = 6;
    constexpr uint32_t kReadPorts = 2;
    constexpr uint32_t kCycles = 3000;

    const std::string path = "/tmp/vulsim_ram_rom_stress.hex";
    {
        std::ofstream fout(path);
        fout << "@0\n";
        fout << "00123\n";
        fout << "0abc0\n";
        fout << "fffff\n";
        fout << "@4\n";
        fout << "00001\n";
        fout << "13579\n";
    }

    VulROM<20, kSize, kReadPorts> rom(path);
    std::array<uint32_t, kSize> model_memory{};
    model_memory[0] = 0x00123u;
    model_memory[1] = 0x0abc0u;
    model_memory[2] = 0xfffffu;
    model_memory[4] = 0x00001u;
    model_memory[5] = 0x13579u;

    std::array<bool, kReadPorts> expect_valid{};
    std::array<uint32_t, kReadPorts> expect_value{};
    std::mt19937_64 rng(0xa5a5f0f012345678ULL);

    for (uint32_t cycle = 0; cycle < kCycles; ++cycle) {
        for (uint32_t port = 0; port < kReadPorts; ++port) {
            if (expect_valid[port]) {
                const uint64_t got = (port == 0) ? as_u64(rom.readdata<0>()) : as_u64(rom.readdata<1>());
                if (got != expect_value[port]) {
                    fail("VulROM randomized model mismatch");
                }
            }
        }

        std::array<bool, kReadPorts> next_expect_valid{};
        std::array<uint32_t, kReadPorts> next_expect_value{};
        for (uint32_t port = 0; port < kReadPorts; ++port) {
            if ((rng() & 1ULL) != 0) {
                const uint64_t addr = rng() % kSize;
                if (port == 0) rom.readreq<0>(Int<3>(addr));
                if (port == 1) rom.readreq<1>(Int<3>(addr));
                next_expect_valid[port] = true;
                next_expect_value[port] = model_memory[addr];
            }
        }

        rom.apply_next_tick();
        expect_valid = next_expect_valid;
        expect_value = next_expect_value;
    }

    for (uint32_t port = 0; port < kReadPorts; ++port) {
        if (expect_valid[port]) {
            const uint64_t got = (port == 0) ? as_u64(rom.readdata<0>()) : as_u64(rom.readdata<1>());
            if (got != expect_value[port]) {
                fail("VulROM final read mismatch");
            }
        }
    }
}

void test_rom_readmemh_parsing() {
    const std::string path = "/tmp/vulsim_ram_rom_parse.hex";
    {
        std::ofstream fout(path);
        fout << "// comment line\n";
        fout << "@0\n";
        fout << "0x0001 0x0002\n";
        fout << "# jump comment\n";
        fout << "@4\n";
        fout << "f_f_f_f\n";
        fout << "0000\n";
    }

    VulROM<16, 8, 1> rom(path);
    rom.readreq<0>(Int<3>(0));
    rom.apply_next_tick();
    if (as_u64(rom.readdata<0>()) != 0x0001u) fail("VulROM parse mismatch at addr 0");

    rom.readreq<0>(Int<3>(1));
    rom.apply_next_tick();
    if (as_u64(rom.readdata<0>()) != 0x0002u) fail("VulROM parse mismatch at addr 1");

    rom.readreq<0>(Int<3>(4));
    rom.apply_next_tick();
    if (as_u64(rom.readdata<0>()) != 0xffffu) fail("VulROM parse mismatch at addr 4");
}

#ifdef NDEBUG
void test_release_invalid_usage_does_not_trip_sanitizers() {
    VulBRAM<Int<8>, 5, 2, 2> ram;
    for (uint32_t i = 0; i < 1024; ++i) {
        ram.readreq<0>(Int<3>(7));
        ram.readreq<1>(Int<3>(6));
        ram.write<0>(Int<3>(7), Int<8>(0xaa));
        ram.write<1>(Int<3>(6), Int<8>(0x55));
        ram.apply_next_tick();
        (void)ram.readdata<0>();
        (void)ram.readdata<1>();
        ram.apply_next_tick();
    }

    VulBRAM1RW<Int<8>, 5> ram1rw;
    for (uint32_t i = 0; i < 1024; ++i) {
        ram1rw.req(Int<3>(7), Int<8>(0x11), false);
        ram1rw.apply_next_tick();
        (void)ram1rw.readdata();
        ram1rw.req(Int<3>(6), Int<8>(0x22), true);
        ram1rw.apply_next_tick();
        (void)ram1rw.readdata();
    }

    const std::string path = "/tmp/vulsim_ram_rom_release_invalid.hex";
    {
        std::ofstream fout(path);
        fout << "00\n";
    }
    VulROM<8, 5, 1> rom(path);
    for (uint32_t i = 0; i < 1024; ++i) {
        rom.readreq<0>(Int<3>(7));
        rom.apply_next_tick();
        (void)rom.readdata<0>();
        rom.apply_next_tick();
        (void)rom.readdata<0>();
    }
}
#endif

} // namespace

int main() {
    test_bram_randomized_model();
    test_bram1rw_randomized_model();
    test_rom_randomized_model();
    test_rom_readmemh_parsing();
#ifdef NDEBUG
    test_release_invalid_usage_does_not_trip_sanitizers();
#endif
    std::cout << "RAM UB stress tests passed!" << std::endl;
    return 0;
}
