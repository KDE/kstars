use HTMesh;
use strict;

printf "%5s %5s %8s %8s %8s %8s %8s %8s %8s %8s %8s\n", 
    qw/size level c_trix r_trix iter/,  "us/circ", "us/rec", "stars-c", "stars-r", "time-c", "time-r";
for my $size ( 1, 10, 90) {
    my $size2 = $size / sqrt(2);
    for my $level(3, 4, 5, 6) {
        my $mesh = new HTMesh($level, 3);
        my $num_triangles = $mesh->total_triangles();
        my ($ra, $dec) = (6.75, -16.72);
        my @pt = ($ra, $dec);
        $mesh->intersect_circle(@pt, $size);
        my $c_trixels = $mesh->result_size();
        my $iter = 1000/ sqrt($c_trixels);
        my $c_time = $mesh->time_circle($iter, @pt, $size );
        my $r_time = $mesh->time_rect($iter, @pt, $size2, $size2);
        my $r_trixels = $mesh->result_size();
        my $c_stars = (126000 / $num_triangles) * $c_trixels;
        my $r_stars = (126000 / $num_triangles) * $r_trixels;
        my $us_per_circ = 1000 * 1000 * $c_time/$iter;
        my $us_per_rect = 1000 * 1000 * $r_time/$iter;
        my $tt1 = $us_per_circ + 5 * $c_stars;
        my $tt2 = $us_per_rect + 5 * $r_stars;
        printf "%5d %5d %8d %8d %8d %8d %8d %8d %8d %8d %8d\n",
            $size, $level, $c_trixels, $r_trixels, $iter, $us_per_circ, $us_per_rect,
            $c_stars, $r_stars, $tt1, $tt2;
    }
}






