#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary=$(mktemp "${TMPDIR:-/tmp}/arcore-initialization.XXXXXX")
trap 'rm -f "$test_binary"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Icpp -Ilibraries/include \
  cpp/ARCoreInitialization.cpp cpp/__tests__/ARCoreInitializationTest.cpp -o "$test_binary"
"$test_binary"
