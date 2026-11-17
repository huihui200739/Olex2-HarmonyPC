#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${OHOS_NATIVE_HOME:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native}"
CMAKE="${CMAKE_BIN:-$SDK/build-tools/cmake/bin/cmake}"
NINJA="${NINJA_BIN:-$SDK/build-tools/cmake/bin/ninja}"
MODE="${1:-ohos}"
JOBS="${BUILD_JOBS:-4}"
case "$MODE" in
  ohos)
    BUILD="$ROOT/out/ohos-arm64"
    "$CMAKE" -S "$ROOT" -B "$BUILD" -G Ninja \
      -DCMAKE_MAKE_PROGRAM="$NINJA" \
      -DCMAKE_TOOLCHAIN_FILE="$SDK/build/cmake/ohos.toolchain.cmake" \
      -DOHOS_ARCH=arm64-v8a -DOHOS_STL=c++_shared -DOHOS_SDK_NATIVE_PLATFORM=ohos-24 \
      -DCMAKE_BUILD_TYPE=Release
    "$CMAKE" --build "$BUILD" --parallel "$JOBS"
    ;;
  host)
    BUILD="$ROOT/out/host"
    "$CMAKE" -S "$ROOT" -B "$BUILD" -G Ninja \
      -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_BUILD_TYPE=Release
    "$CMAKE" --build "$BUILD" --parallel "$JOBS"
    "$(dirname "$CMAKE")/ctest" --test-dir "$BUILD" --output-on-failure
    ;;
  *) echo "Usage: $0 {ohos|host}" >&2; exit 2 ;;
esac
