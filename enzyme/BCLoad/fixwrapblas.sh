OS="`uname`"

case $OS in
  'Darwin') 
sed -i.bu "s/f2c_\(.*\)(/\1_EXTRAWIDTH(/g" BLAS/WRAP/cblaswr.c
    ;;
  *)
sed "s/f2c_\(.*\)(/\1_EXTRAWIDTH(/g" -i BLAS/WRAP/cblaswr.c
;;
esac
echo "#include <stdint.h>" > BLAS/WRAP/stage.c
cat INCLUDE/f2c.h >> BLAS/WRAP/stage.c
cat BLAS/WRAP/cblaswr.c >> BLAS/WRAP/stage.c

case $OS in
  'Darwin') 
sed -i.bu "s/#include \"f2c.h\"//g" BLAS/WRAP/stage.c
    ;;
  *)
sed "s/#include \"f2c.h\"//g" -i BLAS/WRAP/stage.c
;;
esac

sed "s/typedef long int integer;/typedef int32_t integer;/g" BLAS/WRAP/stage.c | sed "s/EXTRAWIDTH//g" > BLAS/WRAP/bclib32.c
sed "s/typedef long int integer;/typedef int64_t integer;/g" BLAS/WRAP/stage.c | sed "s/EXTRAWIDTH/64_/g" > BLAS/WRAP/bclib64.c

for f in SRC/*.c
do
case $OS in
  'Darwin') 
sed -i.bu "s/_(/_EXTRAWIDTH(/g" $f
    ;;
  *)
sed "s/_(/_EXTRAWIDTH(/g" -i $f
;;
esac
echo "#include <stdint.h>" > $f.in
cat INCLUDE/f2c.h >> $f.in
cat $f >> $f.in

case $OS in
  'Darwin') 
sed -i.bu "s/#include \"f2c.h\"//g" $f.in
sed -i.bu "s/#include \"blaswrap.h\"//g" $f.in
    ;;
  *)
sed "s/#include \"f2c.h\"//g" -i $f.in
sed "s/#include \"blaswrap.h\"//g" -i $f.in
;;
esac

sed "s/typedef long int integer;/typedef int32_t integer;/g" $f.in | sed "s/EXTRAWIDTH//g" > $f.i32.c
sed "s/typedef long int integer;/typedef int64_t integer;/g" $f.in | sed "s/EXTRAWIDTH/64_/g" > $f.i64.c
done
