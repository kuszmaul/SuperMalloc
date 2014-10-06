#!/usr/bin/gnuplot -persist
#
#    
#    	G N U P L O T
#    	Version 4.6 patchlevel 3    last modified 2013-04-12 
#    	Build System: Linux x86_64
#    
#    	Copyright (C) 1986-1993, 1998, 2004, 2007-2013
#    	Thomas Williams, Colin Kelley and many others
#    
#    	gnuplot home:     http://www.gnuplot.info
#    	faq, bugs, etc:   type "help FAQ"
#    	immediate help:   type "help"  (plot window: hit 'h')
set terminal pdfcairo dashed transparent fontscale 0.5 size 5.00in, 3.00in 
set output 'new-malloc-test-1K-aggregated-oldallocators.pdf'
unset clip points
set clip one
unset clip two
set bar 1.000000 front
set border 3 front linetype -1 linewidth 1.000
set timefmt z "%d/%m/%y,%H:%M"
set zdata 
set timefmt y "%d/%m/%y,%H:%M"
set ydata 
set timefmt x "%d/%m/%y,%H:%M"
set xdata 
set timefmt cb "%d/%m/%y,%H:%M"
set timefmt y2 "%d/%m/%y,%H:%M"
set y2data 
set timefmt x2 "%d/%m/%y,%H:%M"
set x2data 
set boxwidth
set style fill  empty border
set style rectangle back fc lt -3 fillstyle   solid 1.00 border lt -1
set style circle radius graph 0.02, first 0, 0 
set style ellipse size graph 0.05, 0.03, first 0 angle 0 units xy
set dummy x,y
set format x "% g"
set format y "% g"
set format x2 "% g"
set format y2 "% g"
set format z "% g"
set format cb "% g"
set format r "% g"
set angles radians
unset grid
set raxis
set key title ""
set key inside right top vertical Right noreverse enhanced autotitles nobox
set key noinvert samplen 4 spacing 1 width 0 height 0 
set key maxcolumns 0 maxrows 0
set key noopaque
unset key
unset label
unset arrow
set style increment default
unset style line
unset style arrow
set style histogram clustered gap 2 title  offset character 0, 0, 0
unset logscale
set offsets 0, 0, 0, 0
set pointsize 1
set pointintervalbox 1
set encoding default
unset polar
unset parametric
unset decimalsign
set view 60, 30, 1, 1
set samples 100, 100
set isosamples 10, 10
set surface
unset contour
set clabel '%8.3g'
set mapping cartesian
set datafile separator whitespace
unset hidden3d
set cntrparam order 4
set cntrparam linear
set cntrparam levels auto 5
set cntrparam points 5
set size ratio 0 1,1
set origin 0,0
set style data points
set style function lines
set xzeroaxis linetype -2 linewidth 1.000
set yzeroaxis linetype -2 linewidth 1.000
set zzeroaxis linetype -2 linewidth 1.000
set x2zeroaxis linetype -2 linewidth 1.000
set y2zeroaxis linetype -2 linewidth 1.000
set ticslevel 0.5
set mxtics default
set mytics default
set mztics default
set mx2tics default
set my2tics default
set mcbtics default
set xtics border in scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
set xtics autofreq  norangelimit
set ytics border in scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
set ytics autofreq  norangelimit
set ztics border in scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
set ztics autofreq  norangelimit
set nox2tics
set noy2tics
set cbtics border in scale 1,0.5 mirror norotate  offset character 0, 0, 0 autojustify
set cbtics autofreq  norangelimit
set rtics axis in scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
set rtics autofreq  norangelimit
set title "" 
set title  offset character 0, 0, 0 font "" norotate
set timestamp bottom 
set timestamp "" 
set timestamp  offset character 0, 0, 0 font "" norotate
set rrange [ * : * ] noreverse nowriteback
set trange [ * : * ] noreverse nowriteback
set urange [ * : * ] noreverse nowriteback
set vrange [ * : * ] noreverse nowriteback
set xlabel "producer thread count"
set xlabel  offset character 0, 0, 0 font "" textcolor lt -1 norotate
set x2label "" 
set x2label  offset character 0, 0, 0 font "" textcolor lt -1 norotate
set xrange [ 0.8 : 8.4 ] noreverse nowriteback
set x2range [ * : * ] noreverse nowriteback
set ylabel "malloc()'s per second" 
set ylabel  offset character 0, 0, 0 font "" textcolor lt -1 rotate by -270
set y2label "" 
set y2label  offset character 0, 0, 0 font "" textcolor lt -1 rotate by -270
set yrange [ 0 : * ] noreverse nowriteback
set y2range [ * : * ] noreverse nowriteback
set zlabel "" 
set zlabel  offset character 0, 0, 0 font "" textcolor lt -1 norotate
set zrange [ * : * ] noreverse nowriteback
set cblabel "" 
set cblabel  offset character 0, 0, 0 font "" textcolor lt -1 rotate by -270
set cbrange [ * : * ] noreverse nowriteback
set zero 1e-08
set lmargin  -1
set bmargin  -1
set rmargin  -1
set tmargin  -1
set locale "en_US.utf8"
set pm3d explicit at s
set pm3d scansautomatic
set pm3d interpolate 1,1 flush begin noftriangles nohidden3d corners2color mean
set palette positive nops_allcF maxcolors 0 gamma 1.5 color model RGB 
set palette rgbformulae 7, 5, 15
set colorbox default
set colorbox vertical origin screen 0.9, 0.2, 0 size screen 0.05, 0.6, 0 front bdefault
set style boxplot candles range  1.50 outliers pt 7 separation 1 labels auto unsorted
set loadpath 
set fontpath 
set psdir
set fit noerrorvariables
GNUTERM = "wxt"
set style line 1 lt 1 lc rgb "#FF0000" lw 1 pt 0
set style line 2 lt 1 lc rgb "#006400" lw 1 pt 0
set style line 4 lt 3 lc rgb "#006400" lw 1 pt 0
set style line 3 lt 1 lc rgb "#00008B" lw 1 pt 0
set style line 5 lt 3 lc rgb "#00008B" lw 1 pt 0
set style line 6 lt 1 lc rgb "#CF00CF" lw 1 pt 0
set style line 7 lt 2 lc rgb "#CF00CF" lw 1 pt 0
set style line 8 lt 3 lc rgb "#CF00CF" lw 1 pt 0
# set label 1 "nothreadcache" at 1.8,1.04e7  left  textcolor rgb "#FF0000"
# set label 2 "rtm(predo)"    at 3.9,1.7e7   right textcolor rgb "#006400"
# set label 4 "rtm(nopredo)"  at 4.2,1.55e7  left  textcolor rgb "#006400"
# set label 3 "lock(predo)"   at 3.5,1.3e7   right textcolor rgb "#00008B"
# set label 5 "lock(nopredo)" at 3.65,1.35e7 left  textcolor rgb "#00008B"
set label 6 "dlmalloc"      at 5,4.5e6       right textcolor rgb "#CF00CF"
set label 7 "Hoard"         at 5,1.4e7     right textcolor rgb "#CF00CF"
set label 8 "jemalloc"      at 7.5,3e7   right  textcolor rgb "#CF00CF"
plot \
     'new-malloc-test-1K-aggregated.data' using ($1+.25):17:18:19 with errorbars title "dlmalloc" ls 6,   'new-malloc-test-1K-aggregated.data' using ($1+.25):17 with lines notitle ls 6,\
     'new-malloc-test-1K-aggregated.data' using ($1+.30):20:21:22 with errorbars title "hoard"    ls 7,      'new-malloc-test-1K-aggregated.data' using ($1+.30):20 with lines notitle ls 7,\
     'new-malloc-test-1K-aggregated.data' using ($1+.35):23:24:25 with errorbars title "jemalloc" ls 8, 'new-malloc-test-1K-aggregated.data' using ($1+.35):23 with lines notitle ls 8

#    EOF
