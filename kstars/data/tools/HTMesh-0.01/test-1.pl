use HTMesh;
my $level = 5;
my $size =  1;
my $mesh = new HTMesh($level);
print "\n\nLevel $level mesh has @{[ $mesh->total_triangles() ]} triangles.\n";
print "testing areas of roughly $size degrees.\n";

my ($ra, $dec) = (6.75, -16.72);
my $id = $mesh->lookup_id($ra, $dec);
print "id($ra, $dec) = $id\n";
my $name = $mesh->id_to_name($id);
print "name = $name\n";

print "\nRectangle ...\n";
my @rect = ($ra, $dec, $size, $size);
$mesh->intersect_rect(@rect);
#my $rect_trixels = $mesh->result_size();
#print "Found $rect_trixels triangles\n";

print join("\n", $mesh->results()), "\n";

print "\nCircle ...\n";
my @circle = ($ra, $dec, $size);
$mesh->intersect_circle(@circle);
#my $circ_trixels = $mesh->result_size();
#print "Found $circ_trixels triangles\n";
while ($mesh->has_next()) {
    print $mesh->next_name(), "\n";
}

