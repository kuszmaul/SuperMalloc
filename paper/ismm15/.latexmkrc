#!/usr/bin/env perl

$bibtex = "bibtex -min-crossrefs=9999";

sub gnuplot {
    my $src="$_[0].gnuplot";
    my $dst="$_[0].tex";
    rdb_ensure_file($rule, $src);
    system("gnuplot $src");
}

add_cus_dep("gnuplot", "tex", 0, "gnuplot"); 


sub pdfcrop {
    my $src="$_[0].pptx.pdf";
    my $dst="$_[0].pdf";

    rdb_ensure_file($rule, $src);
    system("pdfcrop $src $dst");
}

add_cus_dep("prn", "pdf", 0, "fixps"); 

sub fixps {
    my $src="$_[0].prn";
    my $ps ="$_[0].ps";
    my $eps ="$_[0].epsi";
    my $dst="$_[0].pdf";

    rdb_ensure_file($rule, $src);
    system("fixps $src -o $ps");
    system("ps2epsi $ps $eps && ps2pdf -dEPSCrop $eps $dst");
   
}

