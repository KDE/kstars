#!/usr/bin/perl
#
# tycdatatomysql_pm.pl   Put star data from Tycho-1/Tycho-2 ASCII data file into a database
#
# CAUTION: Will truncate the table supplied!
#
# NOTE: The Tycho-1 text data file should be supplied via stdin:
#  use  cat <tycho-1 data file> | <this script> to achieve this.
#
# This version of the script takes into account the proper motion of the star for J2000.0 +/- x millenia
use strict;
use DBI;
use HTMesh;
use Math::Trig;
use Math::Complex;

my $ERROR;
my @STARS;

my $VERBOSE = 0;

my $cnt;

# For database handling
my $db_db = shift;
my $db_user = shift;
!($db_db eq "") and !($db_user eq "") or print "USAGE: " . $0 . " <database name> <MySQL Username> [[MySQL Password [Table [Millenia]]]\n" and exit;
my $db_pass = shift;
my $db_tbl = shift;
my $pm_millenia = shift;
($pm_millenia eq "") and $pm_millenia = 10;
if($db_tbl eq "") {
    $db_tbl = "tycho"
}

my $db_query = qq/CREATE DATABASE IF NOT EXISTS `$db_db`;/;
my $db_select_query = qq/USE `$db_db`/;

my $tbl_query = qq/
CREATE TABLE IF NOT EXISTS `$db_tbl` (
  `Trixel` int NOT NULL COMMENT 'Trixel Name',
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
  `Copies` tinyint(8) NOT NULL COMMENT 'Number of Copies of the star',
  PRIMARY KEY  (`UID`),
  UNIQUE KEY `UID` (`UID`),
  KEY `Trixel` (`Trixel`,`PM`,`Mag`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1;/;

my $tbl_trunc_query = qq/TRUNCATE TABLE `$db_tbl`/;

# For the HTMesh
my $level = 3;      # Note: If you change this, change at the TODO below as well.

# Create a new HTMesh, of level $level
my $mesh = new HTMesh($level);

# Get the database handle
my $dbh = DBI->connect("DBI:mysql:", $db_user, $db_pass, { RaiseError => 1, AutoCommit => 0 });

my @fields = qw/Trixel HD RA Dec dRA dDec PM Parallax Mag BV_Index
	Spec_Type Mult Var GName Name Copies/;

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

    my ( $RA1, $Dec1 ) = proper_motion_coords( $star->{RA}, $star->{Dec}, $star->{dRA}, $star->{dDec}, -$pm_millenia * 1000 );
    my ( $RA2, $Dec2 ) = proper_motion_coords( $star->{RA}, $star->{Dec}, $star->{dRA}, $star->{dDec}, $pm_millenia * 1000 );

#    ( $star->{PM} > 1000.0 ) and print "$RA1, $Dec1 : $star->{RA}, $star->{Dec} : $RA2, $Dec2\n";

    my $leftDec;
    my $rightDec;
    my $topRA;
    my $botRA;

    my $verbose = 0;
#    if( $star->{HD} == "91125" ) {
#        $verbose = 1;
#    }
#    else {
#       print $star->{HD} . "\n";
#    }

    $verbose and print "(RA1, RA2, Dec1, Dec2) = ($RA1 ,$RA2 ,$Dec1 ,$Dec2 )\n";

    if( $Dec1 < $Dec2 ) {
        $leftDec = $Dec1;
        $rightDec = $Dec2;
    }
    else {
        $leftDec = $Dec2;
        $rightDec = $Dec1;
    }

    if( $RA1 < $RA2 ) {
        $topRA = $RA1;
        $botRA = $RA2;
    }
    else {
        $topRA = $RA2;
        $botRA = $RA1;
    }

    $verbose and print "leftDec = $leftDec and topRA = $topRA\n";

    my $epsilon = ( 90.0 / 3600.0 ) / 15.0;

    $star->{originalTrixelID} = $mesh->lookup_id( $star->{RA}, $star->{Dec} );

    $verbose and print "Original trixel ID = " . $star->{originalTrixelID};

    my @trixels;
    if( $star->{Name} eq "" && $star->{GName} eq "" ) {
        my $separation = sqrt( hour2deg($botRA - $topRA) * hour2deg($botRA - $topRA) + ($leftDec - $rightDec) * ($leftDec - $rightDec) );
        if( $separation > 50.0 / 60.0 ) {
#            $mesh->intersect_poly4( $botRA, $leftDec,
#                                    $botRA - $epsilon, $leftDec + $epsilon,
#                                    $topRA - $epsilon, $rightDec - $epsilon,
#                                    $topRA, $rightDec);
            $mesh->intersect_poly4( $topRA, $rightDec,
                                    $topRA - $epsilon, $rightDec - $epsilon,
                                    $botRA - $epsilon, $leftDec + $epsilon,
                                    $botRA, $leftDec);

            @trixels = $mesh->results();
        }
        if( !@trixels ) {
           @trixels = $mesh->lookup_id( $star->{RA}, $star->{Dec} );
        }
    }
    else {
        @trixels = $mesh->lookup_id( $star->{RA}, $star->{Dec} );
    }
        
    for(@trixels) {
        my $tid = $_;
        $star->{Trixel} = $tid - 512; # TODO : Change if you change HTMesh level.
        $star->{Copies} = 0;
        if( $star->{originalTrixelID} == $tid ) {
            $star->{Copies} = @trixels;
        }
        $verbose and print "Trixel = " . $star->{Trixel} . "\n";
        my $query ||= qq/INSERT INTO `$db_tbl` (/ .
            join(", ", map {"`$_`"} @fields) .
            qq/) VALUES (/ .
            join(", ", map {"?"} @fields) .
            qq/)/;
        my $sth ||= $dbh->prepare($query);
        $sth->execute(@$star{@fields});
    }
        
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
            \s*(-?\d+\.\d[\d ])\s      # Parallax
            \s*([- \d]\d\.\d\d)\s      # Magnitude
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

sub proper_motion_coords {
    my ( $ra0, $dec0, $pmRA, $pmDec, $years ) = @_;

    my $theta0 = hour2rad($ra0);
    my $lat0   = deg2rad($dec0);

    my $pm = sqrt( $pmRA * $pmRA + $pmDec * $pmDec ) * $years;
    my $dir0 = ( $pm > 0 ) ? atan2( $pmRA, $pmDec ) : atan2( -$pmRA, -$pmDec );
    ( $pm < 0 ) and $pm = - $pm;
#    print "$pm, $dir0\n";

    my $dst = milliarcsecond2rad($pm);

    my $phi0 = pi/2 - $lat0;

    my $lat1   = asin(sin($lat0)*cos($dst) +
                      cos($lat0)*sin($dst)*cos($dir0));
    my $dtheta = atan2(sin($dir0)*sin($dst)*cos($lat0),
                       cos($dst)-sin($lat0)*sin($lat1));

    return (rad2hour($theta0 + $dtheta), rad2deg($lat1));
    #return (rad2milliarcsecond($dtheta) * cos($lat0),
    #        rad2milliarcsecond($lat1 - $lat0));
}

sub hour2rad {
    return deg2rad($_[0] * 15.);
}

sub rad2hour {
    return rad2deg($_[0]) / 15.;
}

sub milliarcsecond2rad {
    return deg2rad( $_[0] / ( 60 * 60 * 1000));
}
sub rad2milliarcsecond {
    return rad2deg( $_[0]) * ( 60 * 60 * 1000);
}
