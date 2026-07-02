#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-/tmp/vulrtl_apiinline_proxy_test_$$}"

require_fixed() {
    local pattern="$1"
    local file="$2"
    if ! rg -Fq "$pattern" "$file"; then
        echo "missing expected pattern: $pattern" >&2
        echo "in file: $file" >&2
        exit 1
    fi
}

reject_regex() {
    local pattern="$1"
    local file="$2"
    if rg -q "$pattern" "$file"; then
        echo "unexpected pattern matched: $pattern" >&2
        echo "in file: $file" >&2
        rg -n "$pattern" "$file" >&2
        exit 1
    fi
}

if [[ -e "$OUT_DIR" ]]; then
    echo "output directory already exists: $OUT_DIR" >&2
    echo "choose another path or remove it before running this test" >&2
    exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build >/dev/null

./build/vulrtlgen \
    --top example/apiinline_proxy/Top.hpp \
    --project example/apiinline_proxy \
    --out "$OUT_DIR" \
    --v2 >/dev/null

HLS="$OUT_DIR/top.logic.cpp"

reject_regex "\\.(enqready|enqreqdy|deqvalid|enqnext|front|deqnext|clrnext|readreq|readdata|write|setnext|get)\\(" "$HLS"
reject_regex "\\b(simple_q|mp_q|rw_mem|bank_mem|coeff_rom|emit_payload|done)\\s*(<[^>]+>)?\\(" "$HLS"

require_fixed "if (simple_q__enqready__)" "$HLS"
require_fixed "simple_q__enqdata__ = __vul_queue_packed;" "$HLS"
require_fixed "simple_q__enqvalid__ = true;" "$HLS"
require_fixed "value.data = simple_q__deqdata__.at<31, 0>();" "$HLS"
require_fixed "simple_q__deqready__ = true;" "$HLS"
require_fixed "simple_q__clrnext__ = true;" "$HLS"

require_fixed "if (mp_q__enqready__ >= 2)" "$HLS"
require_fixed "mp_q__enqdata__[0] = __vul_queue_packed;" "$HLS"
require_fixed "mp_q__enqdata__[1] = __vul_queue_packed;" "$HLS"
require_fixed "mp_q__enqvalid__ = __vul_queue_req;" "$HLS"
require_fixed "value.data = mp_q__deqdata__[0].at<31, 0>();" "$HLS"
require_fixed "mp_q__deqready__ = __vul_queue_req;" "$HLS"
require_fixed "mp_q__clrnext__ = true;" "$HLS"

require_fixed "bram_rw_mem__s1_en = true;" "$HLS"
require_fixed "bram_rw_mem__s1_we = (true);" "$HLS"
require_fixed "bram_rw_mem__s1_wdata = __vul_bram_packed;" "$HLS"
require_fixed "value.data = bram_rw_mem__s2_rdata.at<31, 0>();" "$HLS"

require_fixed "bram_bank_mem__s1_readreq[0] = true;" "$HLS"
require_fixed "bram_bank_mem__s1_readaddr[0] = (addr);" "$HLS"
require_fixed "bram_bank_mem__s1_write[0] = true;" "$HLS"
require_fixed "bram_bank_mem__s1_writedata[0] = __vul_bram_packed;" "$HLS"
require_fixed "value.data = bram_bank_mem__s2_readdata[1].at<31, 0>();" "$HLS"

require_fixed "rom_coeff_rom__s1_readreq[0] = true;" "$HLS"
require_fixed "rom_coeff_rom__s1_readaddr[0] = (addr);" "$HLS"
require_fixed "rom_coeff_rom__s2_readdata[0].template to<uint32_t>()" "$HLS"

require_fixed "emit_payload__vld__ = true;" "$HLS"
require_fixed "emit_payload_payload__.at<31, 0>() = __vul_req_arg_payload.data;" "$HLS"
require_fixed "echoed.data = emit_payload_echoed__.at<31, 0>();" "$HLS"
require_fixed "return emit_payload__rdy__;" "$HLS"
require_fixed "done__vld__ = true;" "$HLS"
require_fixed "done_code__.at<31, 0>() = __vul_req_arg_code;" "$HLS"

echo "apiinline proxy test passed: $HLS"
