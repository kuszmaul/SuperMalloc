FIGS = x86-addresses
FIG_PDFS = $(patsubst %,%_pdftex.pdf,$(FIGS))
FIG_TEXS = $(patsubst %,%_pdftex.tex,$(FIGS))
supermalloc.pdf: supermalloc.tex  $(FIG_PDFS) $(FIG_TEXS)
	pdflatex -shell-escape supermalloc

FIG_MAG_x86-addresses = -m 0.6
%_pdftex.pdf: %.fig
	fig2dev -L pdftex $($(patsubst %.fig,FIG_MAG_%,$<)) $< $@
%_pdftex.tex: %.fig
	fig2dev -L pdftex_t -p $(patsubst %.fig,%_pdftex.pdf,$<) $($(patsubst %.fig,FIG_MAG_%,$<)) $< $@
