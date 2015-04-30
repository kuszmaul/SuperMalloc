default: supermalloc.pdf
supermalloc.pdf: new-malloc-test-1K-tempo-aggregated.pdf
supermalloc.pdf: new-malloc-test-1K-lutestring-aggregated.pdf
supermalloc.pdf: Makefile allpapers.bib
supermalloc.pdf: supermalloc.tex FORCE
	latexmk -pdflatex="pdflatex --shell-escape" -pdf -bibtex $<
.PHONY: FORCE
FORCE:
%.pdf: %.gnuplot %.data
	gnuplot $<
clean:
	rm -f *.aux *.log *.pyg *.bbl *.blg *.fls *.fdb_latexmk supermalloc.pdf

foo.pdf: new-malloc-test-1K-tempo-aggregated.pdf new-malloc-test-1K-lutestring-aggregated.pdf
	pdflatex foo

supermalloc-greyscale.pdf: supermalloc.pdf
	gs -sDEVICE=pdfwrite -sProcessColorModel=DeviceGray -sColorConversionStrategy=Gray -dOverrideICC -o $@ -f $<

SDIR=pldiws15ismmmain-ismmmainismm15-6-p
sources:
	rm -rf $(SDIR)
	mkdir $(SDIR)
	cp supermalloc.tex supermalloc.bbl $(SDIR)
	tar cfz $(SDIR).tar.gz $(SDIR)

measure_gs:
	echo default
	/usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	/usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	/usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	/usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	echo hoard
	LD_PRELOAD=/home/bradley/malloc-implementations/Hoard/src/libhoard.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/Hoard/src/libhoard.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/Hoard/src/libhoard.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/Hoard/src/libhoard.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	echo tbbmalloc
	LD_PRELOAD=/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc_proxy.so.2:/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc.so.2  /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc_proxy.so.2:/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc.so.2  /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc_proxy.so.2:/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc.so.2  /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc_proxy.so.2:/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release/libtbbmalloc.so.2  /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	echo jemalloc
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	echo supermalloc
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null

measure_gs2:
	echo jemalloc
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	echo supermalloc
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
	LD_PRELOAD=/home/bradley/supermalloc/release/lib/libsupermalloc.so /usr/bin/time gs -dBATCH -dNODISPLAY ~/Intel13.pdf > /dev/null
