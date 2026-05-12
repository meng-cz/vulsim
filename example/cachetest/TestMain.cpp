
#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

#include <array>
#include <random>

GLOBAL() {

bool should_hit = false;
UInt<DATA_WIDTH> expected_data = 0;
uint64_t test_tick = 0;

}

// Port

REQUEST(read_s0, ARG(UInt<ADDR_WIDTH>) addr);
REQUEST(refill_s0, ARG(UInt<ADDR_WIDTH>) addr, ARG(UInt<DATA_WIDTH>) data);

SERVICE(readresp_s1, ARG(bool) hit, ARG(UInt<DATA_WIDTH>) data) {
    if (should_hit) {
        if (!hit) {
            printf("%ld: Error: expected a hit but got a miss.\n", test_tick);
            exit(1);
        } else if (data != expected_data) {
            printf("%ld: Error: expected data 0x%016lx but got 0x%016lx.\n", test_tick, expected_data.data[0], data.data[0]);
            exit(1);
        } else {
            // printf("%ld: Hit with correct data: 0x%016lx\n", test_tick, data.data[0]);
        }
    } else {
        if (hit) {
            printf("%ld: Error: expected a miss but got a hit.\n", test_tick);
            exit(1);
        } else {
            // printf("%ld: Miss as expected.\n", test_tick);
        }
    }
}

SIMULATION() {

    struct CacheLine {
        UInt<DATA_WIDTH> data;
        UInt<TAG_WIDTH> tag;
        bool valid;
    };
    constexpr uint64_t kNumLines = 1ULL << INDEX_WIDTH;
    constexpr uint64_t kHotLines = kNumLines < 8 ? kNumLines : 8;

    std::array<CacheLine, kNumLines> cache;
    for (auto& line : cache) {
        line.data = 0;
        line.tag = 0;
        line.valid = false;
    }

    auto build_addr = [](UInt<TAG_WIDTH> tag, UInt<INDEX_WIDTH> index) {
        return (UInt<ADDR_WIDTH>(tag) << INDEX_WIDTH) | UInt<ADDR_WIDTH>(index);
    };

    auto build_data = [](UInt<TAG_WIDTH> tag, UInt<INDEX_WIDTH> index, uint64_t salt) {
        UInt<DATA_WIDTH> mixed = (UInt<DATA_WIDTH>(UInt<ADDR_WIDTH>(tag)) << INDEX_WIDTH) | UInt<DATA_WIDTH>(index);
        return mixed ^ UInt<DATA_WIDTH>(salt * 0x9e3779b97f4a7c15ULL);
    };

    auto model_read = [&](UInt<ADDR_WIDTH> addr, bool& hit, UInt<DATA_WIDTH>& data) {
        UInt<TAG_WIDTH> tag = addr(ADDR_WIDTH - 1, INDEX_WIDTH);
        UInt<INDEX_WIDTH> index = addr(INDEX_WIDTH - 1, 0);
        const auto& line = cache[index.get_u64()];
        hit = line.valid && (line.tag == tag);
        data = line.data;
    };

    auto model_refill = [&](UInt<ADDR_WIDTH> addr, UInt<DATA_WIDTH> data) {
        UInt<TAG_WIDTH> tag = addr(ADDR_WIDTH - 1, INDEX_WIDTH);
        UInt<INDEX_WIDTH> index = addr(INDEX_WIDTH - 1, 0);
        auto& line = cache[index.get_u64()];
        line.data = data;
        line.tag = tag;
        line.valid = true;
    };

    std::mt19937_64 rng(0x5a17deadc0de1234ULL);

    bool should_hit_next = false;
    UInt<DATA_WIDTH> expected_data_next = 0;

    constexpr uint64_t TestTick = 1200UL;

    for (test_tick = 0; test_tick < TestTick; ++test_tick) {
        // 当前拍收到的是上一拍 read_s0 的响应。
        should_hit = should_hit_next;
        expected_data = expected_data_next;
        should_hit_next = false;
        expected_data_next = 0;

        bool do_read = false;
        UInt<ADDR_WIDTH> addr = 0;
        UInt<DATA_WIDTH> refill_data = 0;

        if (test_tick < 2 * kNumLines) {
            // 阶段1：顺序 refill 填满所有 index，且多轮覆盖，建立稳定基础状态。
            UInt<INDEX_WIDTH> index = UInt<INDEX_WIDTH>(test_tick % kNumLines);
            UInt<TAG_WIDTH> tag = UInt<TAG_WIDTH>((test_tick / kNumLines) + 1);
            addr = build_addr(tag, index);
            do_read = false;
            refill_data = build_data(tag, index, test_tick);
        } else if (test_tick < 2 * kNumLines + 4 * kHotLines) {
            // 阶段2：对热点 index 反复读同一 tag，形成连续命中。
            uint64_t hot_i = (test_tick - 2 * kNumLines) % kHotLines;
            UInt<INDEX_WIDTH> index = UInt<INDEX_WIDTH>(hot_i);
            UInt<TAG_WIDTH> tag = UInt<TAG_WIDTH>(2);
            addr = build_addr(tag, index);
            do_read = true;
        } else if (test_tick < 2 * kNumLines + 10 * kHotLines) {
            // 阶段3：同 index 的 tag 冲突，覆盖 miss -> refill -> hit -> miss。
            uint64_t step = (test_tick - (2 * kNumLines + 4 * kHotLines));
            uint64_t phase = step % 5;
            UInt<INDEX_WIDTH> index = UInt<INDEX_WIDTH>((step / 5) % kHotLines);
            UInt<TAG_WIDTH> tag_a = UInt<TAG_WIDTH>(0x11);
            UInt<TAG_WIDTH> tag_b = UInt<TAG_WIDTH>(0x33);
            if (phase == 0) {
                addr = build_addr(tag_a, index);
                do_read = true;   // 一般是 miss（此前被 tag=2 占据）
            } else if (phase == 1) {
                addr = build_addr(tag_a, index);
                do_read = false;  // 写入 A
                refill_data = build_data(tag_a, index, test_tick);
            } else if (phase == 2) {
                addr = build_addr(tag_a, index);
                do_read = true;   // 命中 A
            } else if (phase == 3) {
                addr = build_addr(tag_b, index);
                do_read = true;   // 读 B，触发 miss
            } else {
                addr = build_addr(tag_b, index);
                do_read = false;  // 写入 B，制造替换
                refill_data = build_data(tag_b, index, test_tick);
            }
        } else {
            // 阶段4：可复现随机混合，偏向热点和重复访问。
            bool hot = (rng() & 3ULL) != 0; // 75% 热点访问
            uint64_t index_raw = hot ? (rng() % kHotLines) : (rng() % kNumLines);
            UInt<INDEX_WIDTH> index = UInt<INDEX_WIDTH>(index_raw);
            uint64_t tag_seed = hot ? (rng() % 4ULL) : (rng() % 64ULL);
            UInt<TAG_WIDTH> tag = UInt<TAG_WIDTH>((tag_seed << 2) ^ (index_raw & 0x3ULL));
            addr = build_addr(tag, index);
            do_read = (rng() % 100ULL) < 65ULL; // 读多于写，提升 hit 路径覆盖
            refill_data = build_data(tag, index, test_tick ^ rng());
        }

        if (do_read) {
            bool model_hit = false;
            UInt<DATA_WIDTH> model_data = 0;
            model_read(addr, model_hit, model_data);
            should_hit_next = model_hit;
            expected_data_next = model_data;
            read_s0(addr);
        } else {
            refill_s0(addr, refill_data);
            model_refill(addr, refill_data);
        }

        sim_execute();
        sim_commit();
    }

    // 冲刷水线：检查最后一拍 read_s0 的响应。
    ++test_tick;
    should_hit = should_hit_next;
    expected_data = expected_data_next;
    sim_execute();
    sim_commit();
}
