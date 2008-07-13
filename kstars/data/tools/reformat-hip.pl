#!/usr/bin/perl -w -i.bak - # -*- perl -*-

use strict;
my $ME = $0; $ME =~ s{.*/}{};    #-- program name minus leading path if any

my $USAGE =<<USAGE;
Usage: $ME [options] hip*.dat
Options:
 -c --clean     Delete backup files.
 -e --edit      Actually change the specified files.
 -f --force     Apply changes despite warnings.
 -h --help      Show this usage message.
 -p --pretend   Just show changes that would be made.
 -r --reverse   Convert files in the new format back to the old.
 -u --undo      Undoes changes by moving backup files back to originals.
 -v --verbose   Show filenames in pretend mode.

Converts the format of hipXXX.dat files so that the fixed length gname
comes before the variable length name.

old format: [name[: gname]] | [: gname]
new format: [; gname[; name]]

DANGER: This program edits files in place.  Always use --pretend first.
USAGE

@ARGV or die $USAGE;

my ($REVERSE, $PRETEND, $VERBOSE, $UNDO, $CLEAN, $FORCE, $ABORT, $EDIT);
my $BAK       = ".bak";
my $SEP       = ',';
my $GNAME_LEN = 7;

while (@ARGV and $ARGV[0] =~ s/^-//) {
	my $arg = shift @ARGV;
	if    ($arg =~ /^(r|-reverse)$/) { $REVERSE = 1 }
	elsif ($arg =~ /^(p|-pretend)$/) { $PRETEND = 1 }
	elsif ($arg =~ /^(v|-verbose)$/) { $VERBOSE = 1 }
	elsif ($arg =~ /^(u|-undo)$/   ) { $UNDO    = 1 }
	elsif ($arg =~ /^(c|-clean)$/  ) { $CLEAN   = 1 }
	elsif ($arg =~ /^(f|-force)$/  ) { $FORCE   = 1 }
	elsif ($arg =~ /^(e|-edit)$/   ) { $EDIT    = 1 }
	elsif ($arg =~ /^(h|-help$)/   ) { die $USAGE   }
	elsif ($arg =~ /^$/            ) { last         }
	else { die qq(ERROR: Unrecognized argument: "-$arg":\n$USAGE); }
}

@ARGV or
  die "$ME Error: need to specify at least one hipXXX.dat file\n";

$CLEAN and $UNDO and
  die "$ME Error: can't --clean and --undo at the same time.\n";

$EDIT and $UNDO and
  die "$ME Error: can't --edit and --undo at the same time.\n";

my @files = @ARGV;

#--- Remove backup files
if ($CLEAN and not $EDIT) {
	clean(@files);
}
#--- Copy backups back over 'originals'
elsif ($UNDO) {
	for my $file (@files) {
		my $backup = $file . $BAK;
		-e $backup or do {
			warn "$ME Warning: backup file '$backup' for '$file' does not exist\n";
			next;
		};
		if ($PRETEND) {
			print "$ME would rename: $backup => $file\n";
			next;
		}
		rename( $backup, $file) or 
			warn "$ME Warning: unable to undo changes to '$file': $!\n";
	}
}

#--- Show all lines that would be changed
elsif ($PRETEND) {
	for my $file (@files) {
		open(FILE, $file) or do {
			warn "$ME Warning: could not open($file) $!\n";
			next;
		};
		my $fname = $VERBOSE ? "$file:" : "";
		while (<FILE>) {
			my $new = swap_names($_, $REVERSE);
			$new eq $_ and next;
			print "$fname-$_";
			print "$fname+$new";
		}
		close FILE or die "$ME Warning: could not close($file) $!\n";
	}
}

#--- Edit all @ARGV files in-place via Perl -i flag in line 1.
elsif ($EDIT)  {                    
	while (<>) {
		print swap_names($_, $REVERSE);
	}
}

else {
	die "$ME: Must specify a command: --edit --clean --undo --pretend\n";
}

$EDIT and $CLEAN and clean(@files);

exit;

#=== End of Main code ======================================================

sub clean {
	for my $file (@_) {
		my $backup = $file . $BAK;
		-e $backup or do {
			warn "$ME Warning: backup file '$backup' for '$file' does not exist\n";
			next;
		};
		if ($PRETEND) {
			print "$ME would remove: $backup\n";
			next;
		}
		unlink($backup) or 
			warn "$ME Warning: unable to delete '$backup': $!\n";
	}
}

#---- swap_names($line, $reverse_flag) -------------------------------------
# Change the format of name and gname at the end of the line if it matches
# the format of the hipXXX.dat data files.  Return the line unchanged if
# it starts with "#" or if it is too short.  Aborts if a data line does not
# match the format of hipXXX.dat files.

sub swap_names {
	my ($line, $reverse) = @_;
	return $line if $ABORT;
	$line =~ m/^#/ and return $line;
	my $d1 = substr($line, 0, 72, '');
	my $tail =  $/ x chomp($line);            # $tail contains chomped char(s)
	length($line) > 0 or return $d1 . $tail;

	#-- extreme check of format of a data line
	$FORCE or $d1 =~ m{^(\d{6}\.\d\d)\s  # RA  HHMMSS.SS
					([+-]\d{6}\.\d)\s    # DEC DDMMSS.SS
					([+-]\d{6}\.\d)      # dRA/dt
					([+-]\d{6}\.\d)      # dDec/dt
					(\d{5}\.\d)\s        # Parallax
					([\d-]\d\.\d\d)      # Magnitude
					([\d-]\d\.\d\d)      # B-V index
					([A-Z].)\s           # Spectral Type
					(\d)                 # Multiplicity
     }x or do {
		warn "$ME Warning: This does not look like a hipXXX.dat file\n";
		warn "$ME: Cowardly aborting.  Consider using --force or perhaps --undo.\n";
		$ABORT = 1;
		return  $d1 . $line . $tail;
	};

	my $gname;
	my $name;

	#--- Read names in new format
	if ($line =~ s/^$SEP\s*//) {
		$name  = $line =~ s/\s*$SEP\s*(.*)// ? $1 : "";
		$gname = $line || "";
	}

	#--- Read names in old format
	else {
		$gname = $line =~ s/\s?:\s?(.*)// ? $1 : "";
		$name  = $line || "";
	}
	
	my $names;
	
	#--- Write names in old format
	if ($reverse) {		
		$names = $name;
		$name  and $gname and $names .= " ";
		$gname and $names .= ": $gname";
	}

	#--- Write names in new format
	else {
		$names = "$SEP ";
		$names .= $gname ? $gname : " " x $GNAME_LEN;
		$name and $names .= "$SEP $name";
	}
	return $d1 . $names . $tail;
}

__END__
