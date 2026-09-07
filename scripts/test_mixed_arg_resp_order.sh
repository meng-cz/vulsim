#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE_DIR="${1:-/tmp/vul_mixed_arg_resp_order_test_$$}"
SIM_DIR="${BASE_DIR}_sim"
RTL_DIR="${BASE_DIR}_rtl"
OBJ_DIR="${BASE_DIR}_obj"

for dir in "$SIM_DIR" "$RTL_DIR" "$OBJ_DIR"; do
    if [[ -e "$dir" ]]; then
        echo "output directory already exists: $dir" >&2
        exit 1
    fi
done

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build --target vulsimgen vulrtlgen >/dev/null

COMMON_ARGS=(
    --top example/mixed_arg_resp_order/Top.hpp
    --main example/mixed_arg_resp_order/Main.cpp
    --project example/mixed_arg_resp_order
    --lib build/vullib
)

./build/vulsimgen "${COMMON_ARGS[@]}" --out "$SIM_DIR" >/dev/null
bash "$SIM_DIR/build.sh" >/dev/null
SIM_OUTPUT="$($SIM_DIR/Main)"
if [[ "$SIM_OUTPUT" != *"mixed output: 33"* ]]; then
    echo "unexpected direct simulation output:" >&2
    echo "$SIM_OUTPUT" >&2
    exit 1
fi

./build/vulrtlgen "${COMMON_ARGS[@]}" --out "$RTL_DIR" --v2 >/dev/null

HLS="$RTL_DIR/sim/top.logic.cpp"
if ! rg -Fq \
    'void __vul_req_call_mixed(uint32_t lhs, uint32_t &result, uint32_t rhs)' \
    "$HLS"; then
    echo "generated request helper did not preserve ARG/RESP declaration order" >&2
    rg -n -C 3 '__vul_req_call_mixed' "$HLS" >&2 || true
    exit 1
fi
if ! rg -Fq '__vul_req_call_mixed(lhs, result, rhs);' "$HLS"; then
    echo "generated request call did not preserve source argument order" >&2
    exit 1
fi

verilator --cc --exe --build \
    -j 2 \
    -Wno-TIMESCALEMOD \
    --Mdir "$OBJ_DIR" \
    --top-module Top_sim_top \
    -I"$RTL_DIR" \
    "$RTL_DIR/sim/top.sv" \
    "$RTL_DIR/VulTestMain.cpp" >/dev/null

RTL_OUTPUT="$($OBJ_DIR/VTop_sim_top)"
if [[ "$RTL_OUTPUT" != *"mixed output: 33"* ]]; then
    echo "unexpected generated RTL simulation output:" >&2
    echo "$RTL_OUTPUT" >&2
    exit 1
fi

echo "mixed ARG/RESP order test passed: direct=33 rtl=33"
