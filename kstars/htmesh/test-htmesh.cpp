#include <iostream>

#include "HTMesh.h"
#include "MeshIterator.h"

int main()
{
    int level = 5;
    printf("level = %d\n", level);
    HTMesh *mesh = new HTMesh(level, level);
    mesh->setDebug(17);

    double ra  = 6.75 * 15.;
    double dec = -16.72;

    //Lookup the triangle containing (ra,dec)
    long id          = mesh->index(ra, dec);
    const char *name = mesh->indexToName(id);
    printf("(%8.4f %8.4f): %s\n", ra, dec, name);

    double vr1, vd1, vr2, vd2, vr3, vd3;
    mesh->vertices(id, &vr1, &vd1, &vr2, &vd2, &vr3, &vd3);

    printf("\nThe three vertices of %s are:\n", name);
    printf("    (%6.2f, %6.2f)\n", vr1, vd1);
    printf("    (%6.2f, %6.2f)\n", vr2, vd2);
    printf("    (%6.2f, %6.2f)\n", vr3, vd3);

    printf("\n");
    //Loop over triangles within one degree of (ra,dec)

    double radius = 2.0;
    //printf("Leak test! ...\n");
    for (int ii = 0; ii < 1; ii++)
    {
        mesh->intersect(ra, dec, ra - radius, dec, ra - radius, dec + radius);
        mesh->intersect(ra, dec, ra - radius, dec, ra - radius, dec + radius, ra, dec + radius);

        mesh->intersect(ra, dec, radius);
        //continue;

        MeshIterator iterator(mesh);

        printf("Number of trixels = %d\n\n", mesh->intersectSize());
        printf("Triangles within %5.2f degrees of (%6.2f, %6.2f)\n", radius, ra, dec);

        while (iterator.hasNext())
        {
            printf("%s\n", mesh->indexToName(iterator.next()));
        }
    }

    double ra1, dec1, ra2, dec2;
    ra1  = 275.874939;
    ra2  = 274.882451;
    dec1 = -82.470360;
    dec2 = -82.482475;

    printf("Testing Line intersection: (%f, %f) -->\n", ra1, dec1);
    printf("                           (%f, %f)\n", ra2, dec2);

    mesh->intersect(ra1, dec1, ra2, dec2);
    printf("found %d trixels\n", mesh->intersectSize());

    return 0;
}
