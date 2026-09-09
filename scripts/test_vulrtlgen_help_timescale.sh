#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-/tmp/vulrtlgen_help_timescale_test_$$}"
PROGRESS_LOG="${OUT_DIR}.progress.log"
NORMALIZED_PROGRESS_LOG="${PROGRESS_LOG}.normalized"

if [[ -e "$OUT_DIR" || -e "$PROGRESS_LOG" || -e "$NORMALIZED_PROGRESS_LOG" ]]; then
    echo "output path already exists: $OUT_DIR, $PROGRESS_LOG, or $NORMALIZED_PROGRESS_LOG" >&2
    exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build --target vulrtlgen >/dev/null

HELP_FIRST_LINE="$(build/vulrtlgen --help | sed -n '1p')"
if [[ "$HELP_FIRST_LINE" != "Usage: vulrtlgen "* ]]; then
    echo "vulrtlgen help uses the wrong program name: $HELP_FIRST_LINE" >&2
    exit 1
fi

build/vulrtlgen \
    -t example/childalias/Top.hpp \
    -m example/childalias/Main.cpp \
    -p example/childalias \
    -l vullib \
    -o "$OUT_DIR" \
    -f >"$PROGRESS_LOG"

tr '\r' '\n' <"$PROGRESS_LOG" >"$NORMALIZED_PROGRESS_LOG"

for module in Top_sim_top IncNode_sim_top_node; do
    rg -Fxq "[vulrtlgen] module $module: generating RTL skeleton and API-inline logic" "$NORMALIZED_PROGRESS_LOG"
    rg -Fxq "[vulrtlgen] module $module: RTLzz frontend parsing: s0clang18" "$NORMALIZED_PROGRESS_LOG"
    rg -Fxq "[vulrtlgen] module $module: RTLzz frontend parsing: s11beir" "$NORMALIZED_PROGRESS_LOG"
    rg -q "^\[vulrtlgen\] module $module: RTLzz backend optimization: round 1/[0-9]+$" "$NORMALIZED_PROGRESS_LOG"
done
if ! LC_ALL=C rg -q $'\r\[vulrtlgen\] module .*: RTLzz frontend parsing: s0clang18' "$PROGRESS_LOG"; then
    echo "frontend progress is not written as an in-place carriage-return update" >&2
    exit 1
fi
if ! LC_ALL=C rg -q $'\r\[vulrtlgen\] module .*: RTLzz backend optimization: round 1/' "$PROGRESS_LOG"; then
    echo "backend progress is not written as an in-place carriage-return update" >&2
    exit 1
fi
rg -Fxq \
    "[vulrtlgen] generation complete: output=\"$OUT_DIR\"; top file=\"$OUT_DIR/sim/top.sv\"; top module=Top_sim_top" \
    "$NORMALIZED_PROGRESS_LOG"

mapfile -t SV_FILES < <(find "$OUT_DIR" -type f -name '*.sv' | sort)
if [[ ${#SV_FILES[@]} -eq 0 ]]; then
    echo "vulrtlgen produced no SystemVerilog files" >&2
    exit 1
fi

for file in "${SV_FILES[@]}"; do
    if [[ "$(sed -n '1p' "$file")" != '`timescale 1ns/1ps' ]]; then
        echo "timescale is not the first line of: $file" >&2
        exit 1
    fi
    if [[ "$(rg -c '^`timescale 1ns/1ps$' "$file")" -ne 1 ]]; then
        echo "expected exactly one timescale directive in: $file" >&2
        rg -n 'timescale' "$file" >&2 || true
        exit 1
    fi
done

verilator --lint-only \
    --top-module Top_sim_top \
    -I"$OUT_DIR" \
    "$OUT_DIR/sim/top.sv" \
    "$OUT_DIR/sim/top/node.sv" \
    "$OUT_DIR/queue.sv" \
    "$OUT_DIR/ram_generic.sv" >/dev/null

echo "vulrtlgen help/timescale test passed: ${#SV_FILES[@]} RTL files"
