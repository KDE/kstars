use HTMesh;

if (0) {
    for my $i ( 1..10) {
        print "making new HTMesh ... \n";
        my $htm = HTMesh->new(8);
        sleep 1;
    }
    exit;
}

my $EDGES_ONLY = 0;
my $RECT = 0;
my %LINES;

my $level = 2;
my $size  = 90;
my $mesh = HTMesh->new($level);

my ($ra, $dec) = (6.75, -16.72);

#my ($ra, $dec) = (4.5, -18.5);

my $kstars = `dcopfind -a 'kstars*'`;
chomp $kstars;
$kstars .= " KStarsInterface ";

tell_kstars("eraseLines");
tell_kstars("setRaDec $ra $dec");

if ($RECT) {
    $mesh->intersect_rect($ra, $dec, $size, $size);
}
else {
    $mesh->intersect_circle($ra, $dec, $size);
}

while ( $mesh->has_next() ) {
    my @tri = $mesh->next_triangle();
    kstars_display(@tri);
}
show_edges();

tell_kstars("setRaDec $ra $dec");

#============================================================================
#============================================================================

sub tell_kstars {
    #print "dcop $kstars @_\n";
    return `dcop $kstars @_`;
}

sub kstars_display {
    my @pts;
    for my $i (1..3) {
        my ($ra, $dec) = (shift, shift);
        push @pts, [map sprintf("%.4f", $_), $ra, $dec];
    }
    push @pts, $pts[0];
    for my $i (0..2) {
        my @op = sort {$$a[0] <=> $$b[0] || $$a[1] <=> $$b[1] }
            $pts[$i], $pts[$i+1];
        my $line = "@{$op[0]} @{$op[1]}";
        $LINES{$line}++ or do {
            $EDGES_ONLY or tell_kstars("drawLine", $line, "green");
        };
    }
}
        
sub show_edges {
    for my $line (keys %LINES) {
        #print "$LINES{$line} $line\n";
        $LINES{$line} == 1 and tell_kstars("drawLine", $line, "purple");
    }
}
