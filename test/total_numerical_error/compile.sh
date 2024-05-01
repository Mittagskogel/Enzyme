#!/usr/bin/env sh
set -e

export CXXFLAGS='-fno-exceptions -fpass-plugin=/home/fhoerold/experiments/Enzyme/enzyme/build/Enzyme/ClangEnzyme-19.so -Xclang -load -Xclang /home/fhoerold/experiments/Enzyme/enzyme/build/Enzyme/ClangEnzyme-19.so  -O3 -ffast-math -Rpass=enzyme -include enzyme/fprt/mpfr.h -lmpfr'

export CXXFLAGS="$(pkg-config --libs --cflags mpfr gmp) $CXXFLAGS"

export CXXFLAGS="$CXXFLAGS -g"

export CXXFLAGS="$CXXFLAGS -Wl,-rpath=$(pkg-config --libs-only-L mpfr | sed 's/^-L//g')"

#clang++ $CXXFLAGS /home/fhoerold/experiments/Enzyme/enzyme/Enzyme/Runtimes/FPRT/Trace.cpp -O2 -c -o ./Trace.o
#export CXXFLAGS="$CXXFLAGS ./Trace.o"

#bash -x ./run-one.sh ./benchmarks/rosa.fpcore.c ./driver-enzyme.cpp
clang++ $CXXFLAGS $LDFLAGS ./dexp.cpp -O2 -o ./dexp-enzyme
#./run-all.sh ./driver-enzyme.cpp
