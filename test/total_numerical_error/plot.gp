reset
set terminal png size 1920,1080 enhanced font 'Verdana,18'
#set terminal postscript eps size 8.5,4.78125 enhanced color font 'Verdana,18' linewidth 1

set output 'v.png'

set key outside

set logscale y

set ylabel 'error'
set xlabel 'log_{10}(h)'

plot for [col=2:11] 'v.log' u 1:col with linespoints title columnheader