#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export XWIN_SYSROOT="$PWD/tools/xwin-sysroot"
export LLVM_MINGW_BIN="$PWD/tools/llvm-mingw-20260826-ucrt-ubuntu-22.04-x86_64/bin"
export WINEPREFIX="$PWD/tools/shader-wine"
export WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml='
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXEL_BUILD_PLUGIN=ON -DVOXEL_PLAYTEST="${VOXEL_PLAYTEST:-OFF}" \
  -DCMAKE_TOOLCHAIN_FILE=tools/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$PWD/deps/CommonLibSSE-NG/cmake/toolchain-linux-clangcl.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-clangcl \
  -DVCPKG_OVERLAY_TRIPLETS="$PWD/deps/CommonLibSSE-NG/examples/linux-cross-compile/custom-triplets" \
  -DVCPKG_OVERLAY_PORTS="$PWD/deps/CommonLibSSE-NG/examples/linux-cross-compile/custom-ports" \
  -DCOMMONLIB_PREBUILT=OFF -DENABLE_SKYRIM_VR=OFF -DENABLE_SKYRIM_SE=OFF -DENABLE_SKYRIM_AE=ON
cmake --build build-windows --parallel 4
