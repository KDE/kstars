#include <cstdlib>
#include <iostream>

#include "HTMesh.h"
#include "MeshBuffer.h"
#include "MeshIterator.h"

#include "SpatialVector.h"
#include "SpatialIndex.h"
#include "RangeConvex.h"
#include "HtmRange.h"
#include "HtmRangeIterator.h"

/******************************************************************************
 * Note: There is "complete" checking for duplicate points in the line and
 * polygon intersection routines below.  This may have a slight performance
 * impact on indexing lines and polygons.
 *
 * -- James B. Bowlin
 *****************************************************************************/

HTMesh::HTMesh(int level, int buildLevel, int numBuffers) :
     m_level(level), m_buildLevel(buildLevel), m_numBuffers(numBuffers), htmDebug(0)
{
    name = "HTMesh";
    if (m_buildLevel > 0) {
        if (m_buildLevel > m_level) m_buildLevel = m_level;
        htm = new SpatialIndex(m_level, m_buildLevel);
    }
    else {
        htm = new SpatialIndex(m_level);
    }

    edge = 2. / 3.14;              //  inverse of roughly 1/4 circle
    numTrixels = 8;
    for(int i = m_level; i--;) {
        numTrixels *= 4;
        edge *= 2.0;
    }
    edge = 1.0 / edge;             // roughly length of one edge (radians)
    edge10 = edge / 10.0;
    eps  = 1.0e-6;                 // arbitrary small number

    magicNum = numTrixels;
    degree2Rad = 3.1415926535897932385E0 / 180.0;

    // Allocate MeshBuffers
    m_meshBuffer = (MeshBuffer**) malloc( sizeof(MeshBuffer*) * numBuffers);
    if (m_meshBuffer == NULL) {
        fprintf(stderr, "Out of memory allocating %d MeshBuffers.\n", numBuffers);
        exit(0);
    }
    for (int i = 0; i < numBuffers; i++) {
        m_meshBuffer[i] = new MeshBuffer( this );
    }
}


HTMesh::~HTMesh()
{
    delete htm;
    for ( BufNum i=0; i < m_numBuffers; i++)
        delete m_meshBuffer[i];
    free(m_meshBuffer);
}

Trixel HTMesh::index(double ra, double dec) const
{
    return (Trixel) htm->idByPoint( SpatialVector(ra, dec) ) - magicNum;
}

bool HTMesh::performIntersection(RangeConvex* convex, BufNum bufNum) {

    if ( ! validBufNum(bufNum) )
        return false;

    convex->setOlevel(m_level);
    HtmRange range;
    convex->intersect(htm, &range);
    HtmRangeIterator iterator(&range);

    MeshBuffer* buffer = m_meshBuffer[bufNum];
    buffer->reset();
    while (iterator.hasNext() ) {
        buffer->append( (Trixel) iterator.next() - magicNum);
    }

    if (buffer->error() ) {
        fprintf(stderr, "%s: trixel overflow.\n", name);
        return false;
    };

    return true;
}


// CIRCLE
void HTMesh::intersect(double ra, double dec, double radius, BufNum bufNum)
{
    double d = cos(radius * degree2Rad);
    SpatialConstraint c(SpatialVector(ra, dec), d);
    RangeConvex convex;
    convex.add(c);                      // [ed:RangeConvex::add]

    if ( ! performIntersection(&convex, bufNum) )
        printf("In intersect(%f, %f, %f)\n", ra, dec, radius);
}


// TRIANGLE
void HTMesh::intersect(double ra1, double dec1, double ra2, double dec2,
                       double ra3, double dec3, BufNum bufNum)
{
    if ( fabs(ra1 - ra3) + fabs( dec1 - dec3) < eps )
        return intersect( ra1, dec1, ra2, dec2 );

    else if ( fabs(ra1 - ra2) + fabs(dec1 - dec2) < eps )
        return intersect( ra1, dec1, ra3, dec3 );

    else if ( fabs(ra2 - ra3) + fabs(dec2 - dec3) < eps )
        return intersect( ra1, dec1, ra2, dec2 );

    SpatialVector p1(ra1, dec1);
    SpatialVector p2(ra2, dec2);
    SpatialVector p3(ra3, dec3);
    RangeConvex convex(&p1, &p2, &p3);

    if ( ! performIntersection(&convex, bufNum) )
        printf("In intersect(%f, %f, %f, %f, %f, %f)\n",
                ra1, dec1, ra2, dec2, ra3, dec3);
}


// QUADRILATERAL
void HTMesh::intersect(double ra1, double dec1, double ra2, double dec2,
                       double ra3, double dec3, double ra4, double dec4,
                       BufNum bufNum)
{
    if ( fabs(ra1 - ra4) + fabs(dec1 - dec4) < eps )
        return intersect( ra2, dec2, ra3, dec3, ra4, dec4 );

    else if ( fabs(ra1 - ra2) + fabs(dec1 - dec2) < eps )
        return intersect( ra2, dec2, ra3, dec3, ra4, dec4 );

    else if ( fabs(ra2 - ra3) + fabs(dec2 - dec3) < eps )
        return intersect( ra1, dec1, ra2, dec2, ra4, dec4 );

    else if ( fabs(ra3 - ra4) + fabs(dec3 - dec4) < eps )
        return intersect( ra1, dec1, ra2, dec2, ra4, dec4 );


    SpatialVector p1(ra1, dec1);
    SpatialVector p2(ra2, dec2);
    SpatialVector p3(ra3, dec3);
    SpatialVector p4(ra4, dec4);
    RangeConvex convex( &p1, &p2, &p3, &p4);

    if ( ! performIntersection(&convex, bufNum) )
        printf("In intersect(%f, %f, %f, %f, %f, %f, %f, %f)\n",
               ra1, dec1, ra2, dec2, ra3, dec3, ra4, dec4);
}


void HTMesh::toXYZ(double ra, double dec, double *x, double *y, double *z)
{
    ra  *= degree2Rad;
    dec *= degree2Rad;

    double sinRa  = sin(ra);
    double cosRa  = cos(ra);
    double sinDec = sin(dec);
    double cosDec = cos(dec);

    *x = cosDec * cosRa;
    *y = cosDec * sinRa;
    *z = sinDec;
}


// Intersect a line segment by forming a very thin triangle to use for the
// intersection.  Use cross product to ensure we have a perpendicular vector.

// LINE
void HTMesh::intersect(double ra1, double dec1, double ra2, double dec2,
                       BufNum bufNum)
{
    double x1, y1, z1, x2, y2, z2;
    
    //if (ra1 == 0.0 || ra1 == 180.00) ra1 += 0.1;
    //if (ra2 == 0.0 || ra2 == 180.00) ra2 -= 0.1;
    //if (dec1 == 0.0 ) dec1 += 0.1;
    //if (dec2 == 0.0 ) dec2 -= 0.1;

    // convert to Cartesian. Ugh.
    toXYZ( ra1, dec1, &x1, &y1, &z1);
    toXYZ( ra2, dec2, &x2, &y2, &z2);

    // Check if points are too close
    double len;
    len =  fabs(x1 - x2);
    len += fabs(y1 - y2);
    len += fabs(z1 - z2);

    if (htmDebug > 0 ) {
        printf("htmDebug = %d\n", htmDebug);
        printf("p1 = (%f, %f, %f)\n", x1, y1, z1);
        printf("p2 = (%f, %f, %f)\n", x2, y2, z2);
        printf("edge: %f (radians) %f (degrees)\n", edge, edge / degree2Rad);
        printf("len : %f (radians) %f (degrees)\n", len,  len  / degree2Rad);
    }

    if ( len < edge10 )
        return intersect( ra1, len, bufNum);

    // Cartesian cross product => perpendicular!.  Ugh.
    double cx = y1 * z2 - z1 * y2;
    double cy = z1 * x2 - x1 * z2;
    double cz = x1 * y2 - y1 * x2;

    if ( htmDebug > 0 ) printf("cp  = (%f, %f, %f)\n", cx, cy, cz);

    double norm =  edge10 / ( fabs(cx) + fabs(cy) + fabs(cz) );

    // give it length edge/10
    cx *= norm;
    cy *= norm;
    cz *= norm;

    if ( htmDebug > 0 ) printf("cpn  = (%f, %f, %f)\n", cx, cy, cz);

    // add it to (ra1, dec1)
    cx += x1;
    cy += y1;
    cz += z1;

    if ( htmDebug > 0 ) printf("cpf  = (%f, %f, %f)\n", cx, cy, cz);

    // back to spherical
    norm = sqrt( cx*cx + cy*cy + cz*cz);
    double ra0 = atan2( cy, cx )     / degree2Rad;
    double dec0 = asin( cz / norm )  / degree2Rad;

    if ( htmDebug > 0 ) printf("new ra, dec = (%f, %f)\n", ra0, dec0);

    SpatialVector p1(ra1, dec1);
    SpatialVector p0(ra0, dec0);
    SpatialVector p2(ra2, dec2);
    RangeConvex convex(&p1, &p0, &p2);

    if ( ! performIntersection(&convex, bufNum) )
        printf("In intersect(%f, %f, %f, %f)\n", ra1, dec1, ra2, dec2);
}


MeshBuffer* HTMesh::meshBuffer(BufNum bufNum)
{
    if ( ! validBufNum(bufNum) )
        return 0;
    return m_meshBuffer[ bufNum ];
}


int HTMesh::intersectSize(BufNum bufNum)
{
    if ( ! validBufNum(bufNum) )
        return 0;
    return m_meshBuffer[ bufNum ]->size();
}

void HTMesh::vertices(Trixel id, double *ra1, double *dec1,
                                 double *ra2, double *dec2,
                                 double *ra3, double *dec3)
{
    SpatialVector v1, v2, v3;
    htm->nodeVertex(id + magicNum, v1, v2, v3);
    *ra1  = v1.ra();
    *dec1 = v1.dec();
    *ra2  = v2.ra();
    *dec2 = v2.dec();
    *ra3  = v3.ra();
    *dec3 = v3.dec();
}
