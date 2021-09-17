#!/usr/bin/perl
#
# parse-sac-to-mysql.pl   Put DSO data from Saguaro Astronomy Club's database in the Fence format into a database
#
# CAUTION: Will truncate the table supplied!
#
# SPDX-FileCopyrightText: 2009 James Bowlin <bowlin@mindspring.com> and Akarsh Simha <akarsh.simha@kdemail.net>
# SPDX-License-Identifier: GPL-2.0-or-later

use strict;
use DBI;

my $ERROR;

my $VERBOSE = 1;

# For database handling
my $db_db = shift;
my $db_user = shift;
!($db_db eq "") and !($db_user eq "") or print "USAGE: " . $0 . " <database name> <MySQL Username> [[MySQL Password [Table]]\n" and exit;
my $db_pass = shift;
my $db_tbl = shift or "SAC_DeepSky";

my $db_query = qq/CREATE DATABASE IF NOT EXISTS `$db_db`;/;
my $db_select_query = qq/USE `$db_db`/;

my $tbl_query = qq/
CREATE TABLE IF NOT EXISTS `$db_tbl` (
  `primary` varchar(30) NOT NULL COMMENT 'Primary Designation',
  `secondary` varchar(30) NOT NULL COMMENT 'Alternate Designation',
  `type` smallint NOT NULL COMMENT 'Object Type',
  `const` varchar(3) NOT NULL COMMENT 'Constellation',
  `RA` double NOT NULL COMMENT 'Right Ascension',
  `Dec` double NOT NULL COMMENT 'Declination',
  `mag` float NOT NULL COMMENT 'Magnitude',
  `subr` float NOT NULL COMMENT 'Surface Brightness',
  `U2K` mediumint(9) NULL COMMENT 'Uranometria 2000 chart number',
  `SA2K` smallint(6) NULL COMMENT'SkyAtlas 2000.0 chart number',
  `a` float NOT NULL COMMENT 'Major Axis',
  `b` float NOT NULL COMMENT 'Minor Axis',
  `pa` int NOT NULL COMMENT 'Position Angle',
  `class` varchar(15) NOT NULL COMMENT 'Classification detail',
  `ngc_desc` varchar(30) NOT NULL COMMENT 'NGC Description',
  `notes` varchar(50) NOT NULL COMMENT 'SAC notes',
  PRIMARY KEY  (`primary`),
  KEY `other` (`secondary`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;/;

my $tbl_trunc_query = qq/TRUNCATE TABLE `$db_tbl`/;

# Get the database handle
my $dbh = DBI->connect("DBI:mysql:", $db_user, $db_pass, { RaiseError => 1, AutoCommit => 0 });

my @fields = qw/primary secondary type const RA Dec mag subr U2K SA2K a b pa class ngc_desc notes/;
my %objtypes = qw/1STAR 1 2STAR 17 3STAR 17 4STAR 17 8STAR 17 ASTER 13 OPNCL 3 GLOCL 4 CL+NB 5 G+C+N 5 GX+DN 5 GX+GC 4 BRTNB 5 DRKNB 15 PLNNB 6 SNREM 7 GALXY 8 GALCL 14 LMCCN 5 LMCDN 5 LMCGC 4 LMCOC 3 NONEX 18 SMCCN 5 SMCDN 5 SMCGC 4 SMCOC 3 QUASR 16/;

$dbh->do($db_query);
$dbh->do($db_select_query);
$dbh->do($tbl_query);
$dbh->do($tbl_trunc_query);
$dbh->commit();

while(<>) {
    m/^\s*#/ and do { print if $VERBOSE; next};
    m/\S/ or     do { print if $VERBOSE; next};
    chomp;
    my $dso = kstars_unpack($_) or do {
        warn sprintf("%4d: FORMAT ERROR ($ERROR):\n$_\n", $.);
        next;
    };
    $dso->{line} = $.;

    my $query ||= qq/INSERT INTO `$db_tbl` (/ .
	join(", ", map {"`$_`"} @fields) .
	qq/) VALUES (/ .
	join(", ", map {"?"} @fields) .
	qq/)/;
 
    my $sth ||= $dbh->prepare($query);
 
    $sth->execute(@$dso{@fields});

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
    $VERBOSE and print $line;
    $line =~ m{^\|\s*([^\s\|]+)\s       # Catalog 
             \s*([\d+-\.\sA-Za-z]*?)\s*\|    # Catalog Number / Designation
             (.*?)\s*\|              # Secondary Designation
            ([0-9A-Z+]{5})\|        # Type
             ([A-Z]{3})\|        # Constellation
             (\d\d\s\d\d\.\d)\|     # RA  [String: HH MM.M]
             ([+-]\d{2}\s\d{2})\|     # Dec [String: +/-DD MM]
             ([\s\d]\d\.\d)\|        # Magnitude
             (\d+\.?\d*)\s*\|        # Surface Brightness
             (\d+)\s*\|         # Uranometria 2000.0 Chart Number
             (\d+)\s*\|         # Tirion Sky Atlas 2000.0 Chart Number
             \s*(\d+\.?\d*\s*[ms])?\s*\| # Major Axis
             \s*(\d+\.?\d*\s*[ms])?\s*\| # Minor Axis
             (\d*)\s*\|              # Position Angle
             ([^|]*?)\s*\|           # Class
             ## NOTE: Ignoring fields NSTS, BRSTR and BCHM; TODO: Include them later
             [^\|]*\|[^\|]*\|[^\|]*\|     # Ignore NSTS, BRSTR, BCHM
             ([^|]*?)\s*\|           # NGC Description
             ([^|]*?)\s*\|           # Notes
		  }x or do 
	      {
		  $ERROR = "Positional Error (0-59)";
		  return;
	      };
    my $object = {
        cat => $1,
	catno => $2,
	secondary => $3,
	type_str => $4,
	const => $5,
	ra_str => $6,
	dec_str => $7,
	mag => $8,
	subr => $9,
	U2K => $10,
	SA2K => $11,
	a_str => $12,
	b_str => $13,
	pa => $14,
	class => $15,
	ngc_desc => $16,
	notes => $17
    };

    $object->{primary} = $object->{cat} . " " . $object->{catno};
    $object->{secondary} =~ s/\s+/ /g;
    $object->{type} = $objtypes{$object->{type_str}};
    $object->{ra_str} =~ m/\s*(\d\d) (\d\d.\d)/;# or do { $ERROR="RA String format error: " . $object->{ra_str}; return; };
    $object->{RA} = $1 + $2 / 60.0;
    $object->{dec_str} =~ m/\s*([+-]?\d\d) (\d\d)/;# or do { $ERROR="Dec String format error: " . $object->{dec_str}; return; };
    $object->{Dec} = $1 + $2 / 60.0;
    $object->{a_str} =~ m/\s*(\d*\.?\d*)\s*([ms]?)\s*/;# or do { $ERROR="Major axis format error: " . $object->{a_str}; return; };
    $object->{a} = $1 / (($2 eq 's')? 60.0 : 1.0);
    $object->{b_str} =~ m/\s*(\d*\.?\d*)\s*([ms]?)\s*/;# or do { $ERROR="Minor axis format error: " . $object->{b_str}; return; };
    $object->{b} = $1 / (($2 eq 's')? 60.0 : 1.0);

    return $object;
}
