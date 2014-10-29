onepage.pdf: new-malloc-test-1K-lutestring-aggregated.gnuplot new-malloc-test-1K-lutestring-aggregated.data
onepage.pdf: new-malloc-test-1K-tempo-aggregated.gnuplot      new-malloc-test-1K-tempo-aggregated.data
onepage.pdf: onepage.tex
	gnuplot new-malloc-test-1K-tempo-aggregated.gnuplot 
	gnuplot new-malloc-test-1K-lutestring-aggregated.gnuplot 
	pdflatex --shell-escape $<
