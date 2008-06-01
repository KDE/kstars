#!/usr/bin/perl

use strict;

my $max_len = 0;
my $max_line;
my $max_file;

for my $file (@ARGV) {
    open(FILE, $file) or do {
        warn "Couldn't open($file) $!\n";
        next;
    };
    while (<FILE>) {
        my $len = length($_);
        $len > $max_len or next;
        $max_len = $len;
        $max_file = $file;
        $max_line = $.;
    }
}

print "Max length: $max_len found on line $max_line in file: $max_file\n";

