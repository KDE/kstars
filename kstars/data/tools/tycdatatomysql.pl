#!/usr/bin/perl
#
# datatomysql.pl   Put star data from Tycho-1 ASCII data file into a database
#
# CAUTION: Will truncate the table supplied!
#
# NOTE: The Tycho-1 text data file should be supplied via stdin:
#  use  cat <tycho-1 data file> | <this script> to achieve this.

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
if($db_tbl eq "") {
    $db_tbl = "tycho"
}

my $db_query = qq/CREATE DATABASE IF NOT EXISTS `$db_db`;/;
my $db_select_query = qq/USE `$db_db`/;

my $tbl_query = qq/
CREATE TABLE IF NOT EXISTS `$db_tbl` (
  `Trixel` varchar(14) NOT NULL COMMENT 'Trixel Name',
  `HD` int NULL COMMENT 'HD Catalog Number',
  `RA` double NOT NULL COMMENT 'RA Hours',
  `Dec` double NOT NULL COMMENT 'Declination Degrees',
  `dRA` double NOT NULL COMMENT 'Proper Motion along RA',
  `dDec` double NOT NULL COMMENT 'Proper Motion along Dec',
  `PM` double NOT NULL COMMENT 'Proper Motion (magnitude)',
  `Parallax` double NOT NULL COMMENT 'Parallax',
  `Mag` float NOT NULL COMMENT 'Visual Magnitude',
  `BV_Index` float NOT NULL COMMENT 'B-V Color Index',
  `Spec_Type` char(2) NOT NULL COMMENT 'Spectral Class',
  `Mult` tinyint(1) NOT NULL COMMENT 'Multiple?',
  `Var` tinyint(1) NOT NULL COMMENT 'Variable?',
  `Name` varchar(70) default NULL COMMENT 'Long Name',
  `GName` varchar(15) default NULL COMMENT 'Genetive Name',
  `UID` int(11) NOT NULL auto_increment COMMENT 'Unique ID',
  PRIMARY KEY  (`UID`),
  UNIQUE KEY `UID` (`UID`),
  KEY `Trixel` (`Trixel`,`PM`,`Mag`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1;/;

my $tbl_trunc_query = qq/TRUNCATE TABLE `$db_tbl`/;

# For the HTMesh
my $level = 3;

# Create a new HTMesh, of level $level
my $mesh = new HTMesh($level);

# Get the database handle
my $dbh = DBI->connect("DBI:mysql:", $db_user, $db_pass, { RaiseError => 1, AutoCommit => 0 });

my @fields = qw/Trixel HD RA Dec dRA dDec PM Parallax Mag BV_Index
	Spec_Type Mult Var GName Name/;

$dbh->do($db_query);
$dbh->do($db_select_query);
$dbh->do($tbl_query);
$dbh->do($tbl_trunc_query);
$dbh->commit();

if( $VERBOSE ) {
    for( @fields ) {
        printf $_ . "|";
    }
    printf "\n";
}

while(<>) {
    m/^\s*#/ and do { print if $VERBOSE; next};
    m/\S/ or     do { print if $VERBOSE; next};
    chomp;
    my $star = kstars_unpack($_) or do {
        warn sprintf("%4d: FORMAT ERROR ($ERROR):\n$_\n", $.);
        next;
    };
    $star->{line} = $.;

    if( $VERBOSE ) {
        for(@fields) {
            printf $star->{$_} . "|";
        }
        printf "\n";
    }

    $star->{Trixel} = $mesh->lookup_name($star->{RA}, $star->{Dec});
 
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

sub kstars_unpack {
    my $line = shift;
    chomp $line;

    # Column number 78 demarkates between numeric data and the genetive/long names
    my $s1 = substr($line, 0, 80, "");

    # Comments on the File format:
    # ============================
    # * Most fields are of variable width
    # * All fields are separated by atleast one space and have a non-blank value,
    #   excepting the case of genetive name and long name fields
    #
    # Fields are: 
    # -----------
    # HDNum  RA Dec pmRA pmDec parallax Vmag B-V mult? var? [genetive : full]
    #
    # The regexp used here is as strict as possible on the adherence to field formats
    
    $s1 =~ m{^\s*(\d+)\s               # HD Number
            \s*(\d?\d\.\d{8})\s        # RA Hours
            \s*(-?\d?\d\.\d{8})\s      # DEC DDMMSS.SS
            \s*(-?\d+\.\d\d)\s         # dRA/dt 
            \s*(-?\d+\.\d\d)\s         # dDec/dt
            \s*(-?\d+\.\d\d)\s         # Parallax
            \s([- \d]\d\.\d\d)\s       # Magnitude
            \s*([- ]\d.\d\d)\s         # B-V index
            ([01])\s                   # Multiple?
            ([01])\s                   # Variable?
            ([A-Z ].?|sd)\s            # Spectral Type
            }x or do 
        {
            $ERROR = "Positional Error (0-59)";
            return;
        };
    
    my $star = {
        HD        => $1,
        RA        => $2,
        Dec       => $3,
	dRA       => $4,
	dDec      => $5,
	Parallax  => $6,
        Mag       => $7,
	BV_Index  => $8,
        Mult      => $9,
	Var       => $10,
        Spec_Type => $11,
        line      => $.,
        PM        => sqrt($4 * $4 + $5 * $5),
    };
    
    $line or return $star;
    
#    printf $line . "\n";
    $star->{GName} = (($line =~ s/^([^:\s].*?)(\s:|$)//) ? $1 : "");
    $line =~ s/^://;
    $star->{Name} = (($line =~ s/^\s*(\S.*?)\s*$//) ? $1 : "");
    return $star;
}
