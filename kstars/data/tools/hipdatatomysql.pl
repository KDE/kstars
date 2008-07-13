#!/usr/bin/perl
#
# hipdatatomysql.pl   Put star data from file (in the plain-text data format used by KStars) into a database
#
# CAUTION: Will truncate the table supplied!
#
# NOTE: This script reads Hipparcos text data file on stdin.
#   use cat <hipparcos data file> | <this script>

use strict;
use DBI;
use HTMesh;

my $ERROR;
my @STARS;

my $VERBOSE = 0;

my $cnt;

# For database handling
my $db_db = shift;
my $db_user = shift;
!($db_db eq "") and !($db_user eq "") or print "USAGE: " . $0 . " <database name> <MySQL Username> [[MySQL Password [Table]]\n" and exit;
my $db_pass = shift;
my $db_tbl = shift;

my $db_query = qq/CREATE DATABASE IF NOT EXISTS `$db_db`;/;
my $db_select_query = qq/USE `$db_db`/;

my $tbl_query = qq/
CREATE TABLE IF NOT EXISTS `$db_tbl` (
  `trixel` varchar(14) NOT NULL,
  `ra` double NOT NULL COMMENT 'RA Hours',
  `dec` double NOT NULL COMMENT 'Declination Degrees',
  `dra` double NOT NULL,
  `ddec` double NOT NULL,
  `pm` double NOT NULL,
  `parallax` double NOT NULL,
  `mag` float NOT NULL,
  `bv_index` float NOT NULL,
  `spec_type` char(2) NOT NULL,
  `mult` tinyint(1) NOT NULL,
  `var_range` float default NULL,
  `var_period` float default NULL,
  `name` varchar(70) default NULL,
  `gname` varchar(15) default NULL,
  `UID` int(11) NOT NULL auto_increment,
  PRIMARY KEY  (`UID`),
  UNIQUE KEY `UID` (`UID`),
  KEY `trixel` (`trixel`,`pm`,`mag`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1;/;

my $tbl_trunc_query = qq/TRUNCATE TABLE `allstars`/;

# For the HTMesh
my $level = 3;

# Create a new HTMesh, of level $level
my $mesh = new HTMesh($level);

# Get the database handle
my $dbh = DBI->connect("DBI:mysql:", $db_user, $db_pass, { RaiseError => 1, AutoCommit => 0 });

my @fields = qw/trixel ra dec dra ddec pm parallax mag bv_index
	spec_type mult var_range var_period name gname/;

$dbh->do($db_query);
$dbh->do($db_select_query);
$dbh->do($tbl_query);
$dbh->do($tbl_trunc_query);
$dbh->commit();

while(<>) {
    m/^\s*#/ and do { print if $VERBOSE; next};
    m/\S/ or     do { print if $VERBOSE; next};
    chomp;
    my $star = kstars_unpack($_) or do {
        warn sprintf("%4d: FORMAT ERROR ($ERROR):\n$_\n", $.);
        next;
    };
    $star->{line} = $.;

    $VERBOSE and print_star_line($star);

    $star->{trixel} = $mesh->lookup_name($star->{ra}, $star->{dec});
    $star->{var_range} eq '' and $star->{var_range} = '0';
    $star->{var_period} eq '' and $star->{var_period} = '0';
 
    my $query ||= qq/INSERT INTO `$db_tbl` (/ .
	join(", ", map {"`$_`"} @fields) .
	qq/) VALUES (/ .
	join(", ", map {"?"} @fields) .
	qq/)/;
 
    my $sth ||= $dbh->prepare($query);
 
    $sth->execute(@$star{@fields});

}

$dbh->commit();

$dbh->disconnect();

exit;

#----------------------------------------------------------------------------
#--- Subroutines ------------------------------------------------------------
#----------------------------------------------------------------------------

sub print_star_line {
	my $star = shift;
	#printf "%8.2f ", $star->{pm};
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
	$star->{gname} = (($line =~ s/^\s*,\s*([^\s,][^,]*),?\s*//) ? $1 : "");
        $line =~ s/^[\s,]*//;
	$star->{name}  = $line ? $line : "";
	return $star;
}

sub hms_to_hour {
    my $string = shift;
    $string =~ /^([+-]?)(\d\d)(\d\d)(\d\d(?:\.\d*)?)/ or return;
    my ($h, $m, $s) = ($2, $3, $4);
    my $sign = ($1 eq '-') ? -1 : 1;
    $m += $s / 60;
    return $sign * ($h + $m / 60);
}

sub hms_to_hm {
    my $string = shift;
    $string =~ s/([+-]?\d\d)(\d\d).*/$1:$2/;
    return $string;
}
