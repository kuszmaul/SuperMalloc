#!/usr/bin/env perl

$bibtex = "bibtex -min-crossrefs=9999";

sub gnuplot {
    my $src="$_[0].gnuplot";
    my $dst="$_[0].tex";
    rdb_ensure_file($rule, $src);
    system("gnuplot $src");
}

add_cus_dep("gnuplot", "tex", 0, "gnuplot"); 
