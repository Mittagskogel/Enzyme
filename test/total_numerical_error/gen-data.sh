./dexp | sed 's/rel_err/11-52/g' > v.log

for prec in 10-45 9-38 8-31 8-23 7-16 5-10 8-7 4-3 5-2
do
    echo $prec
    /scratch/fhoerold/spack/opt/spack/linux-rocky8-zen2/gcc-11.3.0/llvm-17.0.6-x42ivozatvlvwim5hemlzcqizyllqe4a/bin/clang -O0 ./dexp.c -o enzyme -I/scratch/fhoerold/spack/opt/spack/linux-rocky8-zen2/gcc-11.3.0/mpfr-4.2.1-pp6hhx4l7nxalxj33r42fcoykghho4ke/include -I/scratch/fhoerold/spack/opt/spack/linux-rocky8-zen2/gcc-11.3.0/gmp-6.2.1-43tzjv5tusp4mxqccogfdt366tlgutyh/include -I/scratch/fhoerold/spack/opt/spack/linux-rocky8-zen2/gcc-11.3.0/llvm-17.0.6-x42ivozatvlvwim5hemlzcqizyllqe4a/lib/clang/17/include/ -L/scratch/fhoerold/spack/opt/spack/linux-rocky8-zen2/gcc-11.3.0/mpfr-4.2.1-pp6hhx4l7nxalxj33r42fcoykghho4ke/lib/ -fpass-plugin=/home/fhoerold/experiments/Enzyme/enzyme/build/Enzyme/ClangEnzyme-17.so -Xclang -load -Xclang /home/fhoerold/experiments/Enzyme/enzyme/build/Enzyme/ClangEnzyme-17.so -mllvm --enzyme-truncate-all="64to${prec}" -include enzyme/fprt/mpfr.h -lm -lmpfr

    paste -d' ' ./v.log <(./enzyme | sed 's/rel_err/'"${prec}"'/g' | awk '{print $2}') > tmp.log
    mv ./tmp.log ./v.log
done
