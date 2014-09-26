#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR=${SCRIPT_DIR}/out
mkdir -p ${OUT_DIR} 
cd ${OUT_DIR} && ${SCRIPT_DIR}/gyp/gyp/gyp ${SCRIPT_DIR}/gyp/mordor.gyp --depth=. -D target_arch=x64 --no-duplicate-basename-check
