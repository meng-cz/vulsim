#include "storage.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <type_traits>
#include <vector>

namespace {

template <typename Reg, typename Value>
void force_reset(Reg &reg, const Value &value) {
    reg._set_reset_value(value);
    reg._reset();
}

struct Payload {
    int32_t tag = 0;
    uint32_t seq = 0;
    std::array<int16_t, 4> lanes{};

    friend bool operator==(const Payload &lhs, const Payload &rhs) {
        return lhs.tag == rhs.tag && lhs.seq == rhs.seq && lhs.lanes == rhs.lanes;
    }
};

Payload make_payload(std::mt19937 &rng, uint32_t seq) {
    std::uniform_int_distribution<int32_t> tag_dist(-2000000, 2000000);
    std::uniform_int_distribution<int32_t> lane_dist(-30000, 30000);

    Payload value;
    value.tag = tag_dist(rng);
    value.seq = seq;
    for (auto &lane : value.lanes) {
        lane = static_cast<int16_t>(lane_dist(rng));
    }
    return value;
}

uint32_t random_below(std::mt19937 &rng, uint32_t limit) {
    assert(limit > 0);
    return static_cast<uint32_t>(rng() % static_cast<std::mt19937::result_type>(limit));
}

template <typename T, uint32_t WRPortNum>
struct RegisterOracle {
    T curr{};
    T next{};

    void reset(const T &value) {
        curr = value;
        next = value;
    }

    void apply_next_tick() {
        curr = next;
    }

    template <uint32_t P>
    void setnext(const T &value) {
        static_assert(P < WRPortNum);
        if constexpr (WRPortNum == 1) {
            next = value;
        } else {
            if (P < best_prio_) {
                next = value;
                best_prio_ = P;
            }
        }
    }

    void end_cycle() {
        best_prio_ = WRPortNum;
    }

private:
    uint32_t best_prio_ = WRPortNum;
};

template <typename T, uint32_t Size, uint32_t WRPortNum>
struct RegisterArrayOracle {
    std::array<T, Size> curr{};
    std::array<T, Size> next{};

    void reset(const T &value) {
        curr.fill(value);
        next.fill(value);
    }

    void reset(const std::array<T, Size> &values) {
        curr = values;
        next = values;
    }

    template <uint32_t P>
    void setnext(uint32_t index, const T &value) {
        static_assert(P < WRPortNum);
        assert(index < Size);
        if constexpr (WRPortNum == 1) {
            next[index] = value;
        } else {
            if (P < best_prio_[index]) {
                next[index] = value;
                best_prio_[index] = P;
            }
        }
    }

    void apply_next_tick() {
        curr = next;
    }

    void end_cycle() {
        best_prio_.fill(WRPortNum);
    }

private:
    std::array<uint32_t, Size> best_prio_ = [] {
        std::array<uint32_t, Size> result{};
        result.fill(WRPortNum);
        return result;
    }();
};

template <typename Impl, typename T>
void check_register_value(const Impl &impl, const T &expected) {
    const T &via_cast = impl;
    assert(via_cast == expected);
    const T &as_ref = impl;
    assert(as_ref == expected);
}

template <typename Impl, typename T, uint32_t WRPortNum>
void run_register_script(Impl &impl, RegisterOracle<T, WRPortNum> &oracle) {
    std::mt19937 rng(20260610u + WRPortNum);
    uint32_t seq = 1;

    for (uint32_t cycle = 0; cycle < 600; ++cycle) {
        if ((cycle % 37u) == 0u) {
            const T reset_value = make_payload(rng, seq++);
            force_reset(impl, reset_value);
            oracle.reset(reset_value);
            check_register_value(impl, oracle.curr);
        }

        if constexpr (WRPortNum == 1) {
            if ((rng() & 1u) != 0u) {
                const T value = make_payload(rng, seq++);
                impl.template setnext<0>(value);
                oracle.template setnext<0>(value);
            }
        } else {
            std::array<uint8_t, WRPortNum> fire_ports{};
            for (uint32_t port = 0; port < WRPortNum; ++port) {
                fire_ports[port] = static_cast<uint8_t>((rng() % 3u) == 0u ? 1u : 0u);
            }
            for (uint32_t port = WRPortNum; port-- > 0;) {
                if (fire_ports[port] == 0) {
                    continue;
                }
                const T value = make_payload(rng, seq++);
                switch (port) {
                    case 0:
                        impl.template setnext<0>(value);
                        oracle.template setnext<0>(value);
                        break;
                    case 1:
                        impl.template setnext<1>(value);
                        oracle.template setnext<1>(value);
                        break;
                    case 2:
                        impl.template setnext<2>(value);
                        oracle.template setnext<2>(value);
                        break;
                    case 3:
                        impl.template setnext<3>(value);
                        oracle.template setnext<3>(value);
                        break;
                    default:
                        assert(false);
                }
            }
        }

        impl.apply_next_tick();
        oracle.apply_next_tick();
        check_register_value(impl, oracle.curr);
        oracle.end_cycle();
    }
}

template <typename Impl, typename T, size_t Size>
void check_array_values(const Impl &impl, const std::array<T, Size> &expected) {
    for (size_t index = 0; index < Size; ++index) {
        assert(impl[static_cast<uint32_t>(index)] == expected[index]);
    }
}

template <typename Impl, typename T, uint32_t Size, uint32_t WRPortNum>
void run_array_script(Impl &impl, RegisterArrayOracle<T, Size, WRPortNum> &oracle) {
    std::mt19937 rng(20260700u + Size * 13u + WRPortNum);
    uint32_t seq = 1;
    std::uniform_int_distribution<uint32_t> index_dist(0, Size - 1);

    for (uint32_t cycle = 0; cycle < 700; ++cycle) {
        const uint32_t reset_mode = random_below(rng, 61u);
        if (reset_mode == 0u) {
            const T value = make_payload(rng, seq++);
            force_reset(impl, value);
            oracle.reset(value);
            check_array_values(impl, oracle.curr);
        } else if (reset_mode == 1u) {
            std::array<T, Size> values{};
            for (auto &value : values) {
                value = make_payload(rng, seq++);
            }
            force_reset(impl, values);
            oracle.reset(values);
            check_array_values(impl, oracle.curr);
        }

        if constexpr (WRPortNum == 1) {
            std::array<uint8_t, Size> used{};
            const uint32_t write_attempts = 1u + random_below(rng, Size * 2u + 1u);
            for (uint32_t attempt = 0; attempt < write_attempts; ++attempt) {
                const uint32_t index = index_dist(rng);
                if (used[index] != 0u) {
                    continue;
                }
                used[index] = 1u;
                const T value = make_payload(rng, seq++);
                impl.template setnext<0>(index, value);
                oracle.template setnext<0>(index, value);
            }
        } else {
            std::array<std::array<uint8_t, Size>, WRPortNum> used{};
            const uint32_t write_attempts = 1u + random_below(rng, Size * 2u + 1u);
            for (uint32_t attempt = 0; attempt < write_attempts; ++attempt) {
                const uint32_t port = random_below(rng, WRPortNum);
                const uint32_t index = index_dist(rng);
                if (used[port][index] != 0u) {
                    continue;
                }
                used[port][index] = 1u;
                const T value = make_payload(rng, seq++);
                switch (port) {
                    case 0:
                        impl.template setnext<0>(index, value);
                        oracle.template setnext<0>(index, value);
                        break;
                    case 1:
                        impl.template setnext<1>(index, value);
                        oracle.template setnext<1>(index, value);
                        break;
                    case 2:
                        impl.template setnext<2>(index, value);
                        oracle.template setnext<2>(index, value);
                        break;
                    case 3:
                        impl.template setnext<3>(index, value);
                        oracle.template setnext<3>(index, value);
                        break;
                    default:
                        assert(false);
                }
            }
        }

        impl.apply_next_tick();
        oracle.apply_next_tick();
        check_array_values(impl, oracle.curr);
        oracle.end_cycle();
    }
}

void test_register_impl1_payload() {
    vulstorage::VulRegisterImpl1<Payload> impl;
    RegisterOracle<Payload, 1> oracle;
    run_register_script<decltype(impl), Payload, 1>(impl, oracle);
}

void test_register_impl_multi_payload() {
    vulstorage::VulRegisterImpl<Payload, 4> impl;
    RegisterOracle<Payload, 4> oracle;
    run_register_script<decltype(impl), Payload, 4>(impl, oracle);
}

void test_register_wrapper_paths() {
    VulRegister<Payload> single;
    RegisterOracle<Payload, 1> single_oracle;
    run_register_script<decltype(single), Payload, 1>(single, single_oracle);

    VulRegister<Payload, 4> multi;
    RegisterOracle<Payload, 4> multi_oracle;
    run_register_script<decltype(multi), Payload, 4>(multi, multi_oracle);
}

void test_array_full_impl_payload() {
    vulstorage::VulRegisterArrayFullImpl<Payload, 16, 4> impl;
    RegisterArrayOracle<Payload, 16, 4> oracle;
    run_array_script<decltype(impl), Payload, 16, 4>(impl, oracle);
}

void test_array_dirty_impl_payload() {
    vulstorage::VulRegisterArrayDirtyImpl<Payload, 17, 4> impl;
    RegisterArrayOracle<Payload, 17, 4> oracle;
    run_array_script<decltype(impl), Payload, 17, 4>(impl, oracle);
}

void test_array_wrapper_threshold_paths() {
    VulRegisterArray<Payload, 1> size1;
    RegisterArrayOracle<Payload, 1, 1> oracle1;
    run_array_script<decltype(size1), Payload, 1, 1>(size1, oracle1);

    VulRegisterArray<Payload, 16, 4> size16;
    RegisterArrayOracle<Payload, 16, 4> oracle16;
    run_array_script<decltype(size16), Payload, 16, 4>(size16, oracle16);

    VulRegisterArray<Payload, 17, 4> size17;
    RegisterArrayOracle<Payload, 17, 4> oracle17;
    run_array_script<decltype(size17), Payload, 17, 4>(size17, oracle17);
}

void test_default_init_paths() {
    static_assert(std::is_default_constructible_v<VulRegister<Payload>>);
    static_assert(std::is_default_constructible_v<VulRegisterArray<Payload, 17, 2>>);

    VulRegister<Payload, 4> reg;
    reg.apply_next_tick();
    check_register_value(reg, Payload{});

    VulRegisterArray<Payload, 16, 4> full_array;
    full_array.apply_next_tick();
    check_array_values(full_array, std::array<Payload, 16>{});

    VulRegisterArray<Payload, 17, 4> dirty_array;
    dirty_array.apply_next_tick();
    check_array_values(dirty_array, std::array<Payload, 17>{});
}

} // namespace

int main() {
    test_default_init_paths();
    test_register_impl1_payload();
    test_register_impl_multi_payload();
    test_register_wrapper_paths();
    test_array_full_impl_payload();
    test_array_dirty_impl_payload();
    test_array_wrapper_threshold_paths();

    std::cout << "storage UB stress tests passed!" << std::endl;
    return 0;
}
