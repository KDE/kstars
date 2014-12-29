use HTMesh;
printf "%5s %5s %8s %8s %8s %8s %8s %8s\n", 
    qw/size level trixels stars iter/,  "us/circ", "time(5)", "time(1)";
for my $size ( 1, 5, 10, 45, 90) {
    printf "%s\n", "-" x 65;
    for my $level( 3, 4, 5, 6) {
        my $mesh = new HTMesh($level, $level);
        my $num_triangles = $mesh->total_triangles();
        my ($ra, $dec) = (6.75, -16.72);
        my @circle = ($ra, $dec, $size);
        $mesh->intersect_circle(@circle);
        my $trixels = $mesh->result_size();
        my $iter = 5000/ sqrt($trixels);
        my $time = $mesh->time_circle($iter, @circle);
        my $stars = (126000 / $num_triangles) * $trixels;
        my $us_per_circ = 1000 * 1000 * $time/$iter;
        my $tt1 = $us_per_circ + 5 * $stars;
        my $tt2 = $us_per_circ + 1 * $stars;
        printf "%5d %5d %8d %8d %8d %8d %8d %8d\n",
            $size, $level, $trixels, $stars, $iter, $us_per_circ, $tt1, $tt2;
    }
    #printf "%s\n", "-" x 72;
}






