default: onepage.pdf
onepage.pdf: new-malloc-test-1K-tempo-aggregated.pdf new-malloc-test-1K-lutestring-aggregated.pdf
onepage.pdf: Makefile allpapers.bib
onepage.pdf: onepage.tex FORCE
	latexmk -pdflatex="pdflatex --shell-escape" -pdf $<
.PHONY: FORCE
FORCE:
%.pdf: %.gnuplot %.data
	gnuplot $<
clean:
	rm -f *.aux *.log *.pyg *.bbl *.blg *.fls *.fdb_latexmk onepage.pdf
