#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE_DIR="${1:-/tmp/vul_output_directory_test_$$}"
SIM_DIR="${BASE_DIR}_sim"
RTL_DIR="${BASE_DIR}_rtl"
SIM_FILE="${BASE_DIR}_sim_file"
RTL_FILE="${BASE_DIR}_rtl_file"

for path in "$SIM_DIR" "$RTL_DIR" "$SIM_FILE" "$RTL_FILE"; do
    if [[ -e "$path" ]]; then
        echo "output test path already exists: $path" >&2
        exit 1
    fi
done

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build --target vulsimgen vulrtlgen >/dev/null

build/vulsimgen --help | rg -q -- '-f, --force'
build/vulrtlgen --help | rg -q -- '-f, --force'

SIM_ARGS=(
    -t example/childalias/Top.hpp
    -m example/childalias/Main.cpp
    -p example/childalias
    -l build/vullib
)
RTL_ARGS=(
    -t example/childalias/Top.hpp
    -p example/childalias
    -l build/vullib
)

touch "$SIM_FILE" "$RTL_FILE"
if build/vulsimgen "${SIM_ARGS[@]}" -f -o "$SIM_FILE" >/dev/null 2>&1; then
    echo "vulsimgen accepted a regular file as its output directory" >&2
    exit 1
fi
if build/vulrtlgen "${RTL_ARGS[@]}" -f -o "$RTL_FILE" >/dev/null 2>&1; then
    echo "vulrtlgen accepted a regular file as its output directory" >&2
    exit 1
fi
test -f "$SIM_FILE"
test -f "$RTL_FILE"

mkdir -p "$SIM_DIR" "$RTL_DIR"
SIM_EMPTY_OUTPUT="$(build/vulsimgen "${SIM_ARGS[@]}" -o "$SIM_DIR" < /dev/null)"
RTL_EMPTY_OUTPUT="$(build/vulrtlgen "${RTL_ARGS[@]}" -o "$RTL_DIR" < /dev/null)"
if [[ "$SIM_EMPTY_OUTPUT" == *"Do you want to clear"* ||
      "$RTL_EMPTY_OUTPUT" == *"Do you want to clear"* ]]; then
    echo "an empty output directory unexpectedly required confirmation" >&2
    exit 1
fi

touch "$SIM_DIR/keep.marker" "$RTL_DIR/keep.marker"
if build/vulsimgen "${SIM_ARGS[@]}" -o "$SIM_DIR" < /dev/null >/dev/null 2>&1; then
    echo "vulsimgen accepted a non-empty directory without confirmation or --force" >&2
    exit 1
fi
if build/vulrtlgen "${RTL_ARGS[@]}" -o "$RTL_DIR" < /dev/null >/dev/null 2>&1; then
    echo "vulrtlgen accepted a non-empty directory without confirmation or --force" >&2
    exit 1
fi
test -f "$SIM_DIR/keep.marker"
test -f "$RTL_DIR/keep.marker"

build/vulsimgen "${SIM_ARGS[@]}" -f -o "$SIM_DIR" < /dev/null >/dev/null
build/vulrtlgen "${RTL_ARGS[@]}" --force -o "$RTL_DIR" < /dev/null >/dev/null
test ! -e "$SIM_DIR/keep.marker"
test ! -e "$RTL_DIR/keep.marker"
test -f "$SIM_DIR/build.sh"
test -f "$RTL_DIR/top.sv"

touch "$SIM_DIR/confirm.marker" "$RTL_DIR/confirm.marker"
printf 'y\n' | build/vulsimgen "${SIM_ARGS[@]}" -o "$SIM_DIR" >/dev/null
printf 'Y\n' | build/vulrtlgen "${RTL_ARGS[@]}" -o "$RTL_DIR" >/dev/null
test ! -e "$SIM_DIR/confirm.marker"
test ! -e "$RTL_DIR/confirm.marker"

echo "output directory handling test passed: empty, reject, force, confirm"
