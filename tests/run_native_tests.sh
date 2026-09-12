#!/usr/bin/env bash
set -euo pipefail
CXX="${CXX:-g++}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RTC_SRC="${RTC_SRC:-${ROOT}/../ESPressio-RTC/src}"
PLATFORM_SRC="${PLATFORM_SRC:-${ROOT}/../ESPressio-Platform/src}"
BUILD="${ROOT}/.native-test-build"
rm -rf "${BUILD}" && mkdir -p "${BUILD}"
"${CXX}" -std=gnu++17 -Wall -Wextra -Werror -pedantic \
  -I"${ROOT}/src" -I"${RTC_SRC}" -I"${PLATFORM_SRC}" \
  "${ROOT}/tests/native/device_test.cpp" -o "${BUILD}/device_test"
"${BUILD}/device_test"
echo "RTC device native tests passed"
