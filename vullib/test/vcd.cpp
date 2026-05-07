#include "vcdrecord.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string read_all(const std::filesystem::path &path) {
    std::ifstream ifs(path);
    assert(ifs.is_open());
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

std::string bits_u64(uint64_t value, uint32_t width) {
    std::string bits;
    bits.reserve(width);
    for (int32_t i = static_cast<int32_t>(width) - 1; i >= 0; --i) {
        bits.push_back(((value >> i) & 1ULL) ? '1' : '0');
    }
    return bits;
}

std::string bits_words(const std::vector<uint64_t> &words, uint32_t width) {
    std::string bits;
    bits.reserve(width);
    for (int32_t i = static_cast<int32_t>(width) - 1; i >= 0; --i) {
        uint32_t wi = static_cast<uint32_t>(i) / 64U;
        uint32_t bi = static_cast<uint32_t>(i) % 64U;
        bits.push_back(((words[wi] >> bi) & 1ULL) ? '1' : '0');
    }
    return bits;
}

void test_width1_manual_close_flush() {
    const std::filesystem::path path = "/tmp/vulsim_vcd_width1_manual.vcd";
    std::filesystem::remove(path);

    GlobalVCDRecord rec;
    const uint32_t clk_id = rec.registe("clk", 1);
    rec.init(path.string(), 10, 0);

    rec.record(clk_id, 0);
    rec.commit();
    rec.record(clk_id, 1);
    rec.commit();

    // write_interval=0: commit 不会写入变化到文件。
    const std::string before_close = read_all(path);
    assert(before_close.find("#10") == std::string::npos);
    assert(before_close.find("#20") == std::string::npos);

    rec.close();

    const std::string content = read_all(path);
    assert(content.find("$timescale 10ns $end") != std::string::npos);
    assert(content.find("$var wire 1 ! clk $end") != std::string::npos);
    assert(content.find("#10\n0!\n") != std::string::npos);
    assert(content.find("#20\n1!\n") != std::string::npos);
}

void test_width64_auto_interval_and_close_tail_flush() {
    const std::filesystem::path path = "/tmp/vulsim_vcd_width64_auto.vcd";
    std::filesystem::remove(path);

    GlobalVCDRecord rec;
    const uint32_t data_id = rec.registe("data64", 64);
    rec.init(path.string(), 5, 2);

    const uint64_t v1 = 0x0123456789abcdefULL;
    const uint64_t v2 = 0xfedcba9876543210ULL;

    rec.record(data_id, v1);
    rec.commit(); // cycle 1, changed

    rec.record(data_id, v1);
    rec.commit(); // cycle 2, unchanged, but触发interval flush

    rec.record(data_id, v2);
    rec.commit(); // cycle 3, changed, wait close flush

    rec.close();

    const std::string content = read_all(path);
    const std::string b1 = "b" + bits_u64(v1, 64) + " !\n";
    const std::string b2 = "b" + bits_u64(v2, 64) + " !\n";

    assert(content.find("$timescale 5ns $end") != std::string::npos);
    assert(content.find("$var wire 64 ! data64 $end") != std::string::npos);
    assert(content.find("#5\n" + b1) != std::string::npos);
    assert(content.find("#10") == std::string::npos); // cycle2无变化
    assert(content.find("#15\n" + b2) != std::string::npos);
}

void test_width_gt64_vector_record() {
    const std::filesystem::path path = "/tmp/vulsim_vcd_width130.vcd";
    std::filesystem::remove(path);

    GlobalVCDRecord rec;
    const uint32_t wide_id = rec.registe("wide130", 130);
    rec.init(path.string(), 1, 1);

    const std::vector<uint64_t> words = {
        0x0123456789abcdefULL,
        0x0f0e0d0c0b0a0908ULL,
        0x0000000000000002ULL,
    };

    rec.record(wide_id, words);
    rec.commit();
    rec.close();

    const std::string content = read_all(path);
    const std::string bits = bits_words(words, 130);

    assert(content.find("$var wire 130 ! wide130 $end") != std::string::npos);
    assert(content.find("#1\n") != std::string::npos);
    assert(content.find("b" + bits + " !\n") != std::string::npos);
}

} // namespace

int main() {
    test_width1_manual_close_flush();
    test_width64_auto_interval_and_close_tail_flush();
    test_width_gt64_vector_record();

    std::cout << "GlobalVCDRecord tests passed!" << std::endl;
    return 0;
}
