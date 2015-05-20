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
set terminal cairolatex pdf colortext transparent size 4.00in, 2.20in font 'ptm,bx' 
set output 'new-malloc-test-1K-tempo-aggregated.tex'
unset clip points
set clip one
unset clip two
set bar 1.000000 front
#set border 31 front linetype -1 linewidth 1.000
set border 0
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
set xtics border out scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
#set xtics autofreq  norangelimit
set xtics ("2" 1,"16" 8,"32" 16,"48" 24,"64" 32)
set ytics border out scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
#set ytics autofreq  norangelimit
set ytics ("\\footnotesize 0" 0, "\\footnotesize$50$M" 50e6, "\\footnotesize$100$M" 100e6, "\\footnotesize $132$M" 132e6)
set ztics border in scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
set ztics autofreq  norangelimit
set nox2tics
set noy2tics
set cbtics border in scale 1,0.5 mirror norotate  offset character 0, 0, 0 autojustify
set cbtics autofreq  norangelimit
set rtics axis in scale 1,0.5 nomirror norotate  offset character 0, 0, 0 autojustify
set rtics autofreq  norangelimit
set title "\\small 16-Core (2 sockets, 32 hardware threads) 2.4GHz Sandy Bridge " 
set title ""
set title  offset character 0, 0, 0 font "" norotate
set timestamp bottom 
set timestamp "" 
set timestamp  offset character 0, 0, 0 font "" norotate
set rrange [ * : * ] noreverse nowriteback
set trange [ * : * ] noreverse nowriteback
set urange [ * : * ] noreverse nowriteback
set vrange [ * : * ] noreverse nowriteback
set xlabel "{\\small Threads}" 
set xlabel  offset character 0, 0, 0 font "" textcolor lt -1 norotate
set x2label "" 
set x2label  offset character 0, 0, 0 font "" textcolor lt -1 norotate
set xrange [ 0 : 32.4 ] noreverse nowriteback
set x2range [ * : * ] noreverse nowriteback
set ylabel "{\\small \\texttt{malloc()}'s per second}" 
set ylabel  offset character 2, 0, 0 font "" textcolor lt -1 rotate by -270
set y2label "" 
set y2label  offset character 0, 0, 0 font "" textcolor lt -1 rotate by -270
set yrange [ 0 : 134e6 ] noreverse nowriteback
set y2range [ * : * ] noreverse nowriteback
set zlabel "" 
set zlabel  offset character 0, 0, 0 font "" textcolor lt -1 norotate
set zrange [ * : * ] noreverse nowriteback
set cblabel "" 
set cblabel  offset character 0, 0, 0 font "" textcolor lt -1 rotate by -270
set cbrange [ * : * ] noreverse nowriteback
set zero 1e-08
set lmargin  7
set bmargin  3
set rmargin  0
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
set style line 2 lt 1 lc rgb "#006400" lw 1 pt 0
set style line 6 lt 1 lc rgb "#A00000" lw 1 pt 0
set style line 7 lt 2 lc rgb "#A000A0" lw 1 pt 0
set style line 8 lt 3 lc rgb "#0000A0" lw 1 pt 0
set style line 9 lt 4 lc rgb "#00A0A0" lw 1 pt 0
set label 4 "\\footnotesize SuperMalloc"      at 15,1.2e8    right textcolor rgb "#006400"
set label 6 "\\footnotesize DLmalloc"         at 18,6e6      right textcolor rgb "#A00000"
set label 7 "\\footnotesize Hoard"            at 32,5e7      offset character 0,0.5 right textcolor rgb "#A000A0"
set arrow 107 from 28,5e7 to 32,17156632 heads size screen 1 ls 7
set label 8 "\\footnotesize JEmalloc"         at 20,5e7      offset character 0,0.5 center textcolor rgb "#0000A0"
set arrow 108 from 20,5e7 to 23,31938839 heads size screen 1 ls 8
set label 9 "\\footnotesize TBBmalloc"        at 15,4.5e7 right textcolor rgb "#00A0A0"
plot "new-malloc-test-1K-tempo-aggregated.data" using 1:2:3:4 with errorbars title "dlmalloc" ls 6,  "new-malloc-test-1K-tempo-aggregated.data" using 1:2 with lines notitle ls 6,\
     "new-malloc-test-1K-tempo-aggregated.data" using 1:5:6:7 with errorbars title "hoard"    ls 7,  "new-malloc-test-1K-tempo-aggregated.data" using 1:5 with lines notitle ls 7,\
     "new-malloc-test-1K-tempo-aggregated.data" using 1:8:9:10 with errorbars title "jemalloc" ls 8, "new-malloc-test-1K-tempo-aggregated.data" using 1:8 with lines notitle ls 8,\
     "new-malloc-test-1K-tempo-aggregated.data" using 1:11:12:13 with errorbars title "tbbmalloc" ls 9, "new-malloc-test-1K-tempo-aggregated.data" using 1:11 with lines notitle ls 9,\
     "new-malloc-test-1K-tempo-aggregated.data" using 1:14:15:16 with errorbars title "supermalloc" ls 2, "new-malloc-test-1K-tempo-aggregated.data" using 1:14 with lines notitle ls 2
#    EOF
