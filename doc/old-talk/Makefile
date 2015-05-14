FIGS = x86-addresses small-number-heap associativity-conflicts
FIG_PDFS = $(patsubst %,%_pdftex.pdf,$(FIGS))
FIG_TEXS = $(patsubst %,%_pdftex.tex,$(FIGS))
supermalloc.pdf: supermalloc.tex  $(FIG_PDFS) $(FIG_TEXS) new-malloc-test-1K-aggregated.pdf
	pdflatex -shell-escape supermalloc

FIG_MAG_small-number-heap = -m 0.8
FIG_MAG_x86-addresses = -m 0.6
FIG_MAG_associativity-conflicts= -m 0.8
%_pdftex.pdf: %.fig
	fig2dev -L pdftex $($(patsubst %.fig,FIG_MAG_%,$<)) $< $@
%_pdftex.tex: %.fig
	fig2dev -L pdftex_t -p $(patsubst %.fig,%_pdftex.pdf,$<) $($(patsubst %.fig,FIG_MAG_%,$<)) $< $@
