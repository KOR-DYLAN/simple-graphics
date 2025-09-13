#!/bin/bash
SCRIPT_PATH="$(readlink -f "$0")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

# aarch64-none-linux-llvm | aarch64-none-linux-gnu
TOOLCHAIN=aarch64-none-linux-llvm
BUILD_TYPE=Release
BUILD=${SCRIPT_DIR}/build-zlib
INSTALL_DIR=${SCRIPT_DIR}/../prebuild/zlib/
NPROC=$(nproc)

pushd zlib

# configure
cmake   -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/../script/toolchain/${TOOLCHAIN}.cmake" \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DZLIB_BUILD_TESTING=OFF ZLIB_BUILD_SHARED=OFF \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
        -S. -B${BUILD}

# build & install
cmake --build ${BUILD} -j ${NPROC} --target install

popd