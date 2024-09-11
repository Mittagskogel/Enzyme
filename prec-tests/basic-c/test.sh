#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

mkdir -p "${SCRIPT_DIR}/build"

clang++ ./src/main.c $(pkg-config --libs --cflags mpfr gmp) -fno-exceptions -fpass-plugin=/scratch/fhrold/riken/Enzyme/enzyme/build/Enzyme/ClangEnzyme-20.so -Xclang -load -Xclang /scratch/fhrold/riken/Enzyme/enzyme/build/Enzyme/ClangEnzyme-20.so -Rpass=enzyme -mllvm -enzyme-truncate-all="64to2-2" -include enzyme/fprt/mpfr.h -o ./build/enzyme

clang ./src/main.c -o ./build/normal

echo "Normal:   " $(./build/normal 0.1 0.2)
echo "Truncated:" $(./build/enzyme 0.1 0.2)
