#!/usr/bin/perl

use strict;

use Math::Trig qw/pi great_circle_distance/;
use Getopt::Long;

my $CULL = 0.007;

my $ME = $0; $ME =~ s{.*/}{};
my $USAGE = <<USAGE;
Usage: $ME <options> file
Options:
    -c --cull X  Cull points less than X radians apart (default: $CULL).
    -h --help    Show this message.
USAGE

@ARGV or die $USAGE;

GetOptions(
    "help"    => sub { print $USAGE; exit},
    "cull=f" => \$CULL,
) or die $USAGE;


my ($ref, $last, $lastRa, $lastDec);

while (<>) {
    chomp;
    m/^#/ and next;
    m/^:(\w*)/ and do {
        $ref = $last = undef;
        print "$1\n";
        next;
    };
    m/(-?\d+\.\d+)\s+(-?\d+\.\d+)\s+(\d)/ or do {
        warn "Warning: line format? : $_";
    };
    my ($ra, $dec, $flag) = ($1, $2, $3);
    #print "[$ra] [$dec]\n";
    my $point = [$ra, $dec];

    if ( defined $ref ) {
        my $dist = distance( $point, $ref);
        my $dRa = ($ra - $lastRa) * 15 * cos($dec);
        my $dDec = $dec - $lastDec;
        my $dir = atan2( $dDec, $dRa ) * 180. / pi;
        $dir < 0.0 and $dir += 360.0;
        printf( "%4d: %d %6.3f %6.3f %6.3f %7.3f \n", 
                $., $flag, $dist, $dRa, $dDec, $dir );
    }
    #$dist < $CULL and not $NODES{"$ra $dec"} and do {
    #    #printf "CULL: $_  %6.4f\n", $dist;
    #    $last = $_;
    #    next;
    #};
    #$dist > $SKIP and do {
    #    $last and print "$last\n";
    #    s/^D/S/;                    # mark point to skip
    #};
    
    #print "$_\n";
    #printf "$_  %6.4f\n", $dist;
    $last = $point;
    $ref = $point;
    $lastRa = $ra;
    $lastDec = $dec;
}

sub distance {
    my ($p1, $p2) = @_;
    return 15 * great_circle_distance(
        $$p1[0] * pi / 12,
        pi/2 - $$p1[1] * pi / 180,
        $$p2[0] * pi / 12,
        pi/2 - $$p2[1] * pi / 180,);
}
                            
