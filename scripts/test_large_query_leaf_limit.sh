#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE_DIR="$ROOT_DIR/tests/fixtures/large_query_leaf_limit"
OUT_DIR="${1:-/tmp/vulrtl_large_query_leaf_limit_$$}"
SIM_DIR="${OUT_DIR}_sim"

if [[ -e "$OUT_DIR" || -e "$SIM_DIR" ]]; then
    echo "output directory already exists: $OUT_DIR or $SIM_DIR" >&2
    exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build --target vulrtlgen vulsimgen -j2 >/dev/null

build/vulsimgen \
    -t "$FIXTURE_DIR/Top.hpp" \
    -m "$FIXTURE_DIR/Main.cpp" \
    -p "$FIXTURE_DIR" \
    -l vullib \
    -o "$SIM_DIR" \
    -f >/dev/null

build/vulrtlgen \
    -t "$FIXTURE_DIR/Top.hpp" \
    -m "$FIXTURE_DIR/Main.cpp" \
    -p "$FIXTURE_DIR" \
    -l vullib \
    -o "$OUT_DIR" \
    -f >/dev/null

if ! rg -Fq "output logic [2047:0] register_file__query__" "$OUT_DIR/sim/top.sv"; then
    echo "missing 2048-bit register-file query port" >&2
    exit 1
fi

if rg -q '__s6_helper___vul_read_reg_regs_[0-9]+___rtlzz_port_rdata_regs' \
    "$OUT_DIR/sim/top.sv"; then
    echo "read helper still clones the complete register-file input port" >&2
    exit 1
fi

verilator --lint-only \
    --top-module Top_sim_top \
    -I"$OUT_DIR" \
    "$OUT_DIR/sim/top.sv" \
    "$OUT_DIR/queue.sv" \
    "$OUT_DIR/ram_generic.sv" >/dev/null

echo "large QUERY leaf-limit regression passed: 32 x 64-bit snapshot"
