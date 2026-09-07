#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-/tmp/vulrtl_child_service_resp_test_$$}"
OBJ_DIR="${OUT_DIR}_obj"

if [[ -e "$OUT_DIR" || -e "$OBJ_DIR" ]]; then
    echo "output directory already exists: $OUT_DIR or $OBJ_DIR" >&2
    echo "choose another path or remove it before running this test" >&2
    exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build --target vulrtlgen >/dev/null

./build/vulrtlgen \
    --top example/childalias/Top.hpp \
    --main example/childalias/Main.cpp \
    --project example/childalias \
    --lib build/vullib \
    --out "$OUT_DIR" \
    --v2 >/dev/null

HLS="$OUT_DIR/sim/top.logic.cpp"
RTL="$OUT_DIR/sim/top.sv"

if ! rg -Fq \
    '(__vul_child_ret_0) = Int<32>(node_inc_out__.at<31, 0>())' \
    "$HLS"; then
    echo "child service response is not unpacked into the caller lvalue" >&2
    rg -n -C 4 '#define node_inc' "$HLS" >&2 || true
    exit 1
fi

if rg -Fq "assign output_data = 32'h0" "$RTL"; then
    echo "child service response was incorrectly constant-folded to zero" >&2
    exit 1
fi

verilator --cc --exe --build \
    -j 2 \
    -Wno-TIMESCALEMOD \
    --Mdir "$OBJ_DIR" \
    --top-module Top_sim_top \
    -I"$OUT_DIR" \
    "$OUT_DIR/sim/top.sv" \
    "$OUT_DIR/sim/top/node.sv" \
    "$OUT_DIR/VulTestMain.cpp" >/dev/null

OUTPUT="$($OBJ_DIR/VTop_sim_top)"
if [[ "$OUTPUT" != *"childalias output: 42"* ]]; then
    echo "unexpected RTL simulation output:" >&2
    echo "$OUTPUT" >&2
    exit 1
fi

echo "child service response test passed: childalias output: 42"
