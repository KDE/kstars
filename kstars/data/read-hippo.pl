#!/usr/bin/perl
#
# read-hippo.pl  For reading kstars hipNNN.dat data files

use strict;
my $ERROR;
my @STARS;
my @DEC_REGIONS = (
#    [90, 50], [50, 20], [20, -20], 
    [-20, -50], [-50, -90]);


while(<>) {
    m/^\s*#/ and do {  next};
    m/\S/ or     do {  next;};
    chomp;
    my $star = kstars_unpack($_) or do {
        warn sprintf("%4d: FORMAT ERROR ($ERROR):\n$_\n", $.);
        next;
    };
    $star->{line} = $.;
    push @STARS, $star;
    #$star->{name} or next;
    #print_star_line($star);
}

@STARS = sort { $b->{pm} <=> $a->{pm} } @STARS;
for my $star ( @STARS ) {
    print_star_line($star);
}

exit;
#my @missing = grep {! $_->{gname} || $_->{gname} =~ /^xx/} @STARS;
 


sub print_star_line {
	my $star = shift;
    printf "%8.2f ", $star->{pm};
	printf "%s %s %s%s%s %s%s%s %s%s",
	@$star{qw/ra_str dec_str dra ddec parallax mag bv_index spec_type mult/};
	my $s2;
	$star->{var_range} || $star->{var_period} and do {
		$s2 = sprintf " %4.2f %6.2f", @$star{qw/var_range var_period/};
	};
	$star->{name} || $star->{gname} and $s2 ||= " " x 12;
	$s2 and print $s2;
	$star->{name} and print $star->{name};
	$star->{name} and $star->{gname} and print " ";
	$star->{gname} and print ": $star->{gname}";
	print "\n";
}
#print_stars(@missing);
exit;

#==========================================================================
#
#


sub print_stars {
    my @stars = sort {$a->{ra} <=> $b->{ra}} @_;
    for my $dec_region (@DEC_REGIONS) {
        my ($top, $bot) = @$dec_region;
        print "\nFrom $top to $bot:\n";
        for my $star ( @stars) {
            my $dec = $star->{dec};
            next unless $dec < $top and $dec >= $bot;
            printf "%4d: %s %s %6.2f %2s %8s %s\n\n",
               @$star{(qw/line ra_hm dec_hm mag spec_type gname name/)};

        }
    }
}

sub kstars_unpack {
    my $line = shift;
	chomp $line;
	my $s1 = substr($line, 0, 60, "");
    $s1 =~ m{^(\d{6}\.\d\d)\s    # RA  HHMMSS.SS
            ([+-]\d{6}\.\d)\s    # DEC DDMMSS.SS
            ([+-]\d{6}\.\d)      # dRA/dt 
            ([+-]\d{6}\.\d)      # dDec/dt
                (\d{5}\.\d)\s    # Parallax
              ([\d-]\d\.\d\d)    # Magnitude
              ([\d-]\d\.\d\d)    # B-V index
              ([A-Z ].|sd)\s     # Spectral Type
              (\d)               # Multiplicity
		  }x or do 
	{
		$ERROR = "Positional Error (0-59)";
		return;
	};
    my $star = {
        ra_str    => $1,
        dec_str   => $2,
		dra       => $3,
		ddec      => $4,
		parallax  => $5,
        mag       => $6,
		bv_index  => $7,
        spec_type => $8,
		mult      => $9,
        line      => $.,
        pm        => sqrt($3 * $3 + $4 * $4),
    };
	$star->{ra}       = hms_to_hour($star->{ra_str});
	$star->{dec}      = hms_to_hour($star->{dec_str}); 
	$star->{ra_hm}    = hms_to_hm($star->{ra_str});
	$star->{dec_hm}   = hms_to_hm($star->{dec_str});

    $line or return $star;

	my $s2 = substr($line, 0, 12, "");
	#print "$s2\n";
	$s2 =~ m/^\s+(\d\.\d\d)?   # var range
			  \s+(\d+\.\d\d)?  # var period
			  /x or do 
	{
		$ERROR = "Variable params (61 - 71)";
		return;
	};
	$star->{var_range}  = $1;
	$star->{var_period} = $2;
	$line or return $star;
	
	$star->{gname} = $line =~ s/\s*:\s*(\S.*)$// ? $1 : "";
	$star->{name}  = $line ? $line : "";
	return $star;
}

sub hms_to_hour {
    my $string = shift;
    $string =~ /^([+-]?\d\d)(\d\d)(\d\d(?:\.\d*)?)/ or return;
    my ($h, $m, $s) = ($1, $2, $3);
    my $sign = $h < 0 ? -1 : 1;
    $h = abs($h);
    $m += $s / 60;
    return $sign * ($h + $m / 60);
}

sub hms_to_hm {
    my $string = shift;
    $string =~ s/([+-]?\d\d)(\d\d).*/$1:$2/;
    return $string;
}
