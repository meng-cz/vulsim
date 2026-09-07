#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE_DIR="${1:-/tmp/vul_cli_error_test_$$}"

if [[ -e "$BASE_DIR" ]]; then
    echo "test path already exists: $BASE_DIR" >&2
    exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B build >/dev/null
cmake --build build --target vulsimgen vulrtlgen >/dev/null

expect_clean_error() {
    local expected="$1"
    shift

    local output
    local status
    set +e
    output="$("$@" 2>&1)"
    status=$?
    set -e

    if [[ $status -ne 1 ]]; then
        echo "expected exit status 1, got $status" >&2
        echo "$output" >&2
        exit 1
    fi
    if [[ "$output" != *"$expected"* ]]; then
        echo "missing expected diagnostic: $expected" >&2
        echo "$output" >&2
        exit 1
    fi
    if [[ "$output" == *"terminate called"* ||
          "$output" == *"Aborted"* ||
          "$output" == *"core dumped"* ]]; then
        echo "CLI terminated through an uncaught exception or signal" >&2
        echo "$output" >&2
        exit 1
    fi
    if [[ "$(rg -c '^ERROR:' <<<"$output")" -ne 1 ]]; then
        echo "expected exactly one structured ERROR diagnostic" >&2
        echo "$output" >&2
        exit 1
    fi
}

MISSING_HEADER=tests/fixtures/cli_missing_header

expect_clean_error \
    "Project root must contain either a header directory or a header.hpp/header.h file" \
    build/vulrtlgen \
        -t "$MISSING_HEADER/Top.hpp" \
        -p "$MISSING_HEADER" \
        -l build/vullib \
        -o "${BASE_DIR}_rtl_missing_header" \
        -f

expect_clean_error \
    "Project root must contain either a header directory or a header.hpp/header.h file" \
    build/vulsimgen \
        -t "$MISSING_HEADER/Top.hpp" \
        -m "$MISSING_HEADER/Main.cpp" \
        -p "$MISSING_HEADER" \
        -l build/vullib \
        -o "${BASE_DIR}_sim_missing_header" \
        -f

expect_clean_error \
    "Library directory does not exist" \
    build/vulrtlgen \
        -t example/childalias/Top.hpp \
        -p example/childalias \
        -l "${BASE_DIR}_missing_vullib" \
        -o "${BASE_DIR}_rtl_missing_vullib" \
        --v1 \
        -f

LONG_OUT="/tmp/$(printf 'x%.0s' {1..300})"
expect_clean_error \
    "filesystem error" \
    build/vulrtlgen \
        -t example/childalias/Top.hpp \
        -p example/childalias \
        -l build/vullib \
        -o "$LONG_OUT" \
        -f

echo "CLI error handling test passed: parse, generation, and filesystem errors exit cleanly"
