#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-/tmp/vulrtl_apiinline_register_test_$$}"

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
    --top example/apiinline_register/Top.hpp \
    --project example/apiinline_register \
    --out "$OUT_DIR" \
    --v2 >/dev/null

HLS="$OUT_DIR/top.logic.cpp"

reject_regex "\\.(setnext|get)\\(" "$HLS"
reject_regex "\\b(scalar_reg|multi_reg|array_reg|payload_reg|payload_array)\\b" "$HLS"
reject_regex "__vul_reg_wdata\\.at<[^>]+>\\(\\) = value\\." "$HLS"

require_fixed "value = rdata_scalar_reg__.at<31, 0>();" "$HLS"

require_fixed "wdata_multi_reg__[0] = __vul_reg_wdata;" "$HLS"
require_fixed "wen_multi_reg__[0] = true;" "$HLS"
require_fixed "wdata_multi_reg__[1] = __vul_reg_wdata;" "$HLS"
require_fixed "wen_multi_reg__[1] = true;" "$HLS"
require_fixed "wdata_multi_reg__[2] = __vul_reg_wdata;" "$HLS"
require_fixed "wen_multi_reg__[2] = true;" "$HLS"

require_fixed "value = rdata_array_reg__[idx].at<15, 0>();" "$HLS"
require_fixed "wdata_array_reg__[idx][0] = __vul_reg_wdata;" "$HLS"
require_fixed "wen_array_reg__[idx][0] = true;" "$HLS"
require_fixed "wdata_array_reg__[(idx + 1) & (REG_ARRAY_SIZE - 1)][1] = __vul_reg_wdata;" "$HLS"
require_fixed "wen_array_reg__[(idx + 1) & (REG_ARRAY_SIZE - 1)][1] = true;" "$HLS"

require_fixed "value.meta.tag = rdata_payload_reg__.at<39, 32>();" "$HLS"
require_fixed "__vul_reg_wdata.at<39, 32>() = __vul_reg_value.meta.tag;" "$HLS"
require_fixed "__vul_reg_wdata.at<40, 40>() = __vul_reg_value.meta.valid;" "$HLS"
require_fixed "wdata_payload_reg__ = __vul_reg_wdata;" "$HLS"
require_fixed "wen_payload_reg__ = true;" "$HLS"

require_fixed "value.meta.tag = rdata_payload_array__[1].at<39, 32>();" "$HLS"
require_fixed "value.meta.valid = rdata_payload_array__[2].at<40, 40>();" "$HLS"
require_fixed "wdata_payload_array__[idx][1] = __vul_reg_wdata;" "$HLS"
require_fixed "wen_payload_array__[idx][1] = true;" "$HLS"

echo "apiinline register test passed: $HLS"
