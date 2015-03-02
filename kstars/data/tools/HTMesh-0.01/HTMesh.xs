#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

#include "ppport.h"

#include <iostream>

#include "SpatialVector.h"
#include "SpatialIndex.h"
#include "RangeConvex.h"
#include "HtmRange.h"
#include "HtmRangeIterator.h"


class HTMesh {
int level, build_level;
SpatialIndex *htm;
char name_buffer[128];
HtmRange* range;
HtmRangeIterator* iterator;
long numTriangles;

public:
        HTMesh(int level_in, int build_level_in) {
            level = level_in;
            build_level = build_level_in;
            if (build_level > 0) {
                if (build_level > level) {
                    build_level = level;
                }
                htm = new SpatialIndex(level, build_level);
            }
            else {
                htm = new SpatialIndex(level);
            }
            
            numTriangles = 8;
            for (int i = level; i--;) {
                numTriangles *= 4;
            }
        }
       
        ~HTMesh() {
            delete htm;
        }

        /* long lookupId(double ra, double dec) { */
        /*      return htm->idByPoint(ra * 15.0, dec); */
        /* } */

        const char* idToName(long id) {
            htm->nameById(id, name_buffer);
            return name_buffer;
        }

        /* const char* lookupName(double ra, double dec) { */
        /*     long id = lookupId(ra, dec); */
        /*     return idToName(id); */
        /* } */

        void intersectCircle(double ra, double dec, double rad) {
            ra = ra * 15.0;
            RangeConvex convex;
            double d = cos( 3.1415926535897932385E0 * rad/180.0);
            SpatialConstraint c(SpatialVector(ra, dec), d);
            convex.add(c); // [ed:RangeConvex::add]
            convex.setOlevel(level);
            range = new HtmRange();
            convex.intersect(htm, range); // commit fec61ac2c400e7138425961e779fed3bbf5687a5 removed unmanipulated bool parameter from RangeConvex::intersect -- asimha
            iterator = new HtmRangeIterator(range);
        }

        void intersectRectangle(double ra, double dec, double dra, double ddec) {
            dra = dra / 15.0;
            double ra1 = ra - dra;
            double ra2 = ra + dra;
            double dec1 = dec - ddec;
            double dec2 = dec + ddec;
            intersectPolygon4(ra1, dec1, ra1, dec2, ra2, dec2, ra2, dec1);
        }
        
        void intersectPolygon4(double ra1, double dec1, double ra2, double dec2,
                               double ra3, double dec3, double ra4, double dec4) {
            SpatialVector* corner[4];
            corner[0] = new SpatialVector(ra1 * 15.0, dec1);
            corner[1] = new SpatialVector(ra2 * 15.0, dec2);
            corner[2] = new SpatialVector(ra3 * 15.0, dec3);
            corner[3] = new SpatialVector(ra4 * 15.0, dec4);
            RangeConvex* convex = new RangeConvex(corner[0], corner[1], corner[2], corner[3]);
            convex->setOlevel(level);
            range = new HtmRange();
            convex->intersect(htm, range); // commit fec61ac2c400e7138425961e779fed3bbf5687a5 removed unmanipulated bool parameter from RangeConvex::intersect -- asimha
            iterator = new HtmRangeIterator(range);
        }

        double timeRectangle(int iter, double ra, double dec, double dra, double ddec) {
            int i = iter;
            time_t t0 = clock();
            while(i--) {
                intersectRectangle(ra, dec, dra, ddec);
            }
            time_t t1 = clock();
            double nsec = (double)(t1-t0)/(double)CLOCKS_PER_SEC ;
            return nsec;
        }

        double timeCircle(int iter, double ra, double dec, double rad) {
            int i = iter;
            time_t t0 = clock();
            while(i--) {
                intersectCircle(ra, dec, rad);
            }
            time_t t1 = clock();
            double nsec = (double)(t1-t0)/(double)CLOCKS_PER_SEC ;
            return nsec;
        }

        HtmRangeIterator* getIterator() {
            return iterator;
        }

        SpatialIndex* getHtm() {
            return htm;
        }

        bool hasNext() {
            return iterator->hasNext();
        }

        long nextId() {
            return iterator->next();
        }
        const char * nextName() {
            return iterator->nextSymbolic(name_buffer);
        }

        long totalTriangles() {
            return numTriangles;
        }

        int resultSize() {
            HtmRangeIterator* iter;
            int size = 0;
            iter = new HtmRangeIterator(range);
            while (iter->hasNext()) {
                size++;
                iter->next();
            }
            return size;
        }
};

MODULE = HTMesh		PACKAGE = HTMesh

HTMesh *
HTMesh::new(int level, int build_level=0)

void
HTMesh::DESTROY()

void
HTMesh::intersect_rect(double ra, double dec, double dra, double ddec)
CODE:
    THIS->intersectRectangle(ra, dec, dra, ddec);

void
HTMesh::intersect_poly4(double ra1, double dec1, double ra2, double dec2, double ra3, double dec3, double ra4, double dec4)
CODE:
    THIS->intersectPolygon4(ra1, dec1, ra2, dec2, ra3, dec3, ra4, dec4);

void
HTMesh::intersect_circle(double ra, double dec, double rad)
CODE:
    THIS->intersectCircle(ra, dec, rad);
    
double
HTMesh::time_rect(int iterations, double ra, double dec, double dra, double ddec)
CODE:
    RETVAL = THIS->timeRectangle(iterations, ra, dec, dra, ddec);
OUTPUT:
    RETVAL
    
double
HTMesh::time_circle(int iter, double ra, double dec, double rad)
CODE:
    RETVAL = THIS->timeCircle(iter, ra, dec, rad);
OUTPUT:
    RETVAL
    
void
HTMesh::results()
PPCODE:
        HtmRangeIterator* iterator = THIS->getIterator();
        while ( iterator->hasNext() ) {
                XPUSHs(sv_2mortal(newSVnv(iterator->next())));
        }

void
HTMesh::next_triangle()
PPCODE:
        long id = THIS->nextId();
        SpatialIndex* htm = THIS->getHtm();
        SpatialVector v1, v2, v3;
        htm->nodeVertex(id, v1, v2, v3);
        
        XPUSHs(sv_2mortal(newSVnv( v1.ra() / 15.0 )));
        XPUSHs(sv_2mortal(newSVnv( v1.dec() )));
        
        XPUSHs(sv_2mortal(newSVnv( v2.ra() / 15.0 )));
        XPUSHs(sv_2mortal(newSVnv( v2.dec() )));

        XPUSHs(sv_2mortal(newSVnv( v3.ra() / 15.0 )));
        XPUSHs(sv_2mortal(newSVnv( v3.dec() )));

        XPUSHs(sv_2mortal(newSVnv( id )));

bool
HTMesh::has_next()
CODE:
    RETVAL = THIS->hasNext();
OUTPUT:
    RETVAL

const char*
HTMesh::id_to_name(long id)
CODE:
    RETVAL = THIS->idToName(id);
OUTPUT:
    RETVAL

long
HTMesh::next_id()
CODE:
    RETVAL = THIS->nextId();
OUTPUT:
    RETVAL

const char *
HTMesh::next_name()
CODE:
    RETVAL = THIS->nextName();
OUTPUT:
    RETVAL

long
HTMesh::total_triangles()
CODE:
    RETVAL = THIS->totalTriangles();
OUTPUT:
    RETVAL

int
HTMesh::result_size()
CODE:
    RETVAL = THIS->resultSize();
OUTPUT:
    RETVAL
    
