FIGS = x86-addresses small-number-heap associativity-conflicts chunk-array folio-problem
FIG_PDFS = $(patsubst %,%_pdftex.pdf,$(FIGS))
FIG_TEXS = $(patsubst %,%_pdftex.tex,$(FIGS))
DIR = $(notdir $(shell pwd))
$(DIR).pdf: $(DIR).tex
	pdflatex --shell-escape $<
.PHONY: FORCE
FORCE:

FIG2FIGFILES = $(patsubst %,%_pdftex.tex,$(FIGS)) $(patsubst %,%_pdftex.pdf,$(FIGS))

$(DIR).pdf: new-malloc-test-1K-lutestring-aggregated.tex \
            new-malloc-test-1K-tempo-aggregated.tex \
            $(FIG2FIGFILES)

FIG_MAG_folio-problem = -m 0.6
FIG_MAG_chunk-array = -m 0.6
FIG_MAG_small-number-heap = -m 0.8
FIG_MAG_x86-addresses = -m 0.6
FIG_MAG_associativity-conflicts= -m 0.8
FIG_MAG_cache-architecture= -m 0.7
%_pdftex.pdf: %.fig Makefile
	fig2dev -L pdftex $($(patsubst %.fig,FIG_MAG_%,$<)) $< $@
%_pdftex.tex: %.fig Makefile
	fig2dev -L pdftex_t -p $(patsubst %.fig,%_pdftex.pdf,$<) $($(patsubst %.fig,FIG_MAG_%,$<)) $< $@
%.tex: %.gnuplot %.data
	gnuplot $<
