#include "HTMesh.h"
#include <iostream>

int main() {
  HTMesh *mesh = new HTMesh( 5, 5 );
  MeshIterator *iterator;

  double ra=6.75;
  double dec=-16.72;

  //Lookup the triangle containing (ra,dec)
  long id = mesh->lookupId( ra, dec );
  const char *name = mesh->idToName( id );

  std::cout << "(" << ra << "," << dec << "): " << name << std::endl;

  //Loop over triangles within one degree of (ra,dec)
  mesh->intersectCircle( ra, dec, 1.0 );
  iterator = mesh->iterator();

  std::cout << std::endl;
  std::cout << "Triangles within 1 degree of (" << ra << "," << dec << "):" << std::endl;

  while ( iterator->hasNext() ) {
    std::cout << mesh->idToName( iterator->next() ) << std::endl;
  }

  return 0;
}
