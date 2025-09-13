#!/bin/bash
SCRIPT_PATH="$(readlink -f "$0")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

# aarch64-none-linux-llvm | aarch64-none-linux-gnu
TOOLCHAIN=aarch64-none-linux-llvm
BUILD_TYPE=Release
BUILD=${SCRIPT_DIR}/build-libpng
INSTALL_DIR=${SCRIPT_DIR}/../prebuild/libpng/
NPROC=$(nproc)
ZLIB_PREFIX="${SCRIPT_DIR}/../prebuild/zlib"

pushd libpng

# configure
cmake   -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/../script/toolchain/${TOOLCHAIN}.cmake" \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DPNG_SHARED=OFF PNG_TESTS=OFF \
        -DPNG_EXECUTABLES=OFF \
        -DZLIB_ROOT=${ZLIB_PREFIX} \
        -DZLIB_LIBRARY=${ZLIB_PREFIX}/lib \
        -DZLIB_INCLUDE_DIR=${ZLIB_PREFIX}/include \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
        -S. -B${BUILD}

# build & install
cmake --build ${BUILD} -j ${NPROC} --target install

popd