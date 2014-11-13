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
