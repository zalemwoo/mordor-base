#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR=${SCRIPT_DIR}/out

if [ "${GYP_PATH}" == "" ]; then
    GYP_PATH=${SCRIPT_DIR}/../../tools/gyp
fi

mkdir -p ${OUT_DIR} 
cd ${OUT_DIR} && ${GYP_PATH}/gyp ${SCRIPT_DIR}/gyp/mordor.gyp --depth=. -D target_arch=x64 -D library=static_library -D boost_path=../.. -D openssl_path=../../openssl/openssl/include $@
