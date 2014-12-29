use strict;

use HTMesh;
my $level = 6;
my $size =  170;


my $mesh = new HTMesh($level);
my $num_triangles = $mesh->total_triangles();
print "\n\nLevel $level mesh has $num_triangles triangles.\n";
print "testing areas of roughly $size degrees.\n";

my $magic_1 = $mesh->total_triangles();
my ($ra, $dec) = (6.75, -16.72);

my $min = $num_triangles;
my $max = 0;
my $cnt = 0;

print "\nCircle ...\n";
my @circle = ($ra, $dec, $size);
$mesh->intersect_circle(@circle);
while ($mesh->has_next()) {
    my $id = $mesh->next_id() - $magic_1;
    #print $id - $magic_1, "\n";
    $min = $id if $min > $id;
    $max = $id if $max < $id;
    $cnt++;
}
print "Cnt: $cnt\nMin: $min\nMax: $max\n";


