#!/usr/bin/perl -w
use strict;
use LWP::Simple;

if ( $#ARGV != 0 ) {
    print "Usage: ./check_url.pl <filename>\n\n";
    exit(1);
}

open( INF, $ARGV[0] );

while ( my $line = <INF> ) {
    chomp($line);
    my $i1 = index( $line, ":" );
    my $i2 = index( $line, "http" );

    if ( $i1 > -1 && $i2 > -1 ) {
	my $name = substr( $line, 0, $i1 );
	my $url = substr( $line, $i2 );

	&img_check($name, $url);
    }
}

close( INF );
exit;


sub img_check {
    my $name = shift;
    my $url = shift;

    if (head($url)) {
    } else {
	print "$name: $url\n";
    }
} # end sub url_check 
