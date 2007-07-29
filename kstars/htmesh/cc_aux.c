
#include <stdio.h>
#include <math.h>
#include <string.h>

/*
 * cc_name2ID
 * cc_isinside
 * cc_startpane
 * cc_parseVectors
 * cc_vector2ID
 * cc_radec2ID
 * cc_ID2name
 */

#define IDSIZE 64

typedef double				float64;

#ifdef _WIN32
typedef __int64				int64;
typedef unsigned __int64	uint64;

typedef __int32 int32;
typedef unsigned __int32 uint32;
#else
typedef long long			int64;
typedef unsigned long long	uint64;

typedef long int32;
typedef unsigned long uint32;
#define IDHIGHBIT  0x8000000000000000LL
#define IDHIGHBIT2 0x4000000000000000LL
#endif

uint64 cc_name2ID(const char *name);

#define HTMNAMEMAX         32

/**
// extern  "C" { cc_parseVectors(char *spec, double *ra, double *dec); } ;
**/

static const double gEpsilon = 1.0E-15;

/**
// const float64 gEpsilon = 1.0E-15;
**/

#define prmag(lab,v) {\
  double prtmp = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];\
  if (prtmp > 1+gEpsilon || prtmp < 1-gEpsilon) printf("%s: Mag^2 = %f\n", lab, prtmp);}

double anchor[][3] = {
  {0.0L,  0.0L,  1.0L},  /* 0 */
  {1.0L,  0.0L,  0.0L},  /* 1 */
  {0.0L,  1.0L,  0.0L},  /* 2 */
  {-1.0L,  0.0L,  0.0L}, /* 3 */
  {0.0L, -1.0L,  0.0L},  /* 4 */
  {0.0L,  0.0L, -1.0L}   /* 5 */
};

struct _bases {
	char *name;
	int ID;
	int v1, v2, v3;
} bases[] = {
	{"S2", 10, 3, 5, 4},
	{"N1", 13, 4, 0, 3},
	{"S1", 9, 2, 5, 3},
	{"N2", 14, 3, 0, 2},
	{"S3", 11, 4,5,1},
	{"N0", 12, 1, 0, 4},
	{"S0", 8, 1, 5, 2},
	{"N3", 15, 2, 0, 1}
};

int S_indexes[][3] = {
  {1, 5, 2}, /* S0 */
  {2, 5, 3}, /* S1 */
  {3, 5, 4}, /* S2 */
  {4, 5, 1}  /* S3 */
};
int N_indexes[][3] = {
  {1, 0, 4}, /* N0 */
  {4, 0, 3}, /* N1 */
  {3, 0, 2}, /* N2 */
  {2, 0, 1}  /* N3 */
};
  
/**
// to save one indirection: int cc_sel[] = {10, 13, 9, 14, 11, 12, 8, 15};
**/
int cc_isinside(double *p, double *v1, double *v2, double *v3);

static int cc_errcode;

int cc_startpane(
		 double *v1, double *v2, double *v3,
		 double xin, double yin, double zin, char *name)
{
  int ix = (xin > 0 ? 4 : 0) + (yin > 0 ? 2 : 0) + (zin > 0 ? 1 : 0);
  double *tvec;
  char *s;
  int baseID;

  /**
  // printf("Startpane %f %f %f, (%d)\n", xin, yin, zin, ix);
**/
  baseID = bases[ix].ID;
	
  tvec = anchor[bases[ix].v1];
  v1[0] = tvec[0];
  v1[1] = tvec[1];
  v1[2] = tvec[2];

  tvec = anchor[bases[ix].v2];
  v2[0] = tvec[0];
  v2[1] = tvec[1];
  v2[2] = tvec[2];

  tvec = anchor[bases[ix].v3];
  v3[0] = tvec[0];
  v3[1] = tvec[1];
  v3[2] = tvec[2];

  s = bases[ix].name;
  name[0] = *s++;
  name[1] = *s++;
  name[2] = '\0';
  return baseID;
}

int cc_parseVectors(char *spec, int *level, double *ra, double *dec)
{
  char blank = ' ';
  char *s = spec;
	
  int scanned;

  cc_errcode = 0;

  /**
  // no token bs, we'll do it ourselves fast
  // only deal with 'J2000 6 41.4 47.9' style for now
  // States/Tokens: ^, J2000, whitespace (ws), int, ws, double, ws, double
  // assume some leading whitespace. Only blanks for now
  **/

  while(*s == blank) s++;
  if (*s++ != 'J') { cc_errcode = 1; return cc_errcode;}
  if (*s++ != '2') { cc_errcode = 1; return cc_errcode;}
  if (*s++ != '0') { cc_errcode = 1; return cc_errcode;}
  if (*s++ != '0') { cc_errcode = 1; return cc_errcode;}
  if (*s++ != '0') { cc_errcode = 1; return cc_errcode;}
  while(*s == blank) s++;

  scanned = sscanf(s, "%d %lf %lf", level, ra, dec);
  if (scanned != 3) { cc_errcode = 2; return cc_errcode;}

  return cc_errcode;
}

const double cc_Pi = 3.1415926535897932385E0 ;
const double cc_Pr = 3.1415926535897932385E0/180.0; 
double cc_Epsilon = 1.0E-15;
double cc_sqrt3    = 1.7320508075688772935;

#define m4_midpoint(v1, v2, w, tmp){\
	w[0] = v1[0] + v2[0]; w[1] = v1[1] + v2[1]; w[2] = v1[2] + v2[2]; \
	tmp = sqrt(w[0] * w[0] + w[1] * w[1] + w[2]*w[2]);\
	w[0] /= tmp; w[1] /= tmp; w[2] /= tmp;}

#define copy_vec(d, s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }




uint64 cc_vector2ID(double x, double y, double z, int depth)
{
  uint64 rstat = 0;
  int startID;
  char name[80];
  int len = 0;

  double v1[3], v2[3], v0[3];
  double w1[3], w2[3], w0[3];
  double p[3];
  double dtmp;

  p[0] = x;
  p[1] = y;
  p[2] = z;

  /**
  // Get the ID of the level0 triangle, and its starting vertices
  **/

  startID = cc_startpane(v0, v1, v2, x, y, z, name);
  len = 2;
  /**
  // Start searching for the children
  **/
  while(depth-- > 0){
    m4_midpoint(v0, v1, w2, dtmp);
    m4_midpoint(v1, v2, w0, dtmp);
    m4_midpoint(v2, v0, w1, dtmp);

/*     prmag("v0", v0); */
/*     prmag("v1", v1); */
/*     prmag("v2", v2); */

/*     prmag("w0", w0); */
/*     prmag("w1", w1); */
/*     prmag("w2", w2); */

/*     prmag("The point", p); */


    if (cc_isinside(p, v0, w2, w1)) {
      name[len++] = '0';
      copy_vec(v1, w2);
      copy_vec(v2, w1);
    }
    else if (cc_isinside(p, v1, w0, w2)) {
      name[len++] = '1';
      copy_vec(v0, v1);
      copy_vec(v1, w0);
      copy_vec(v2, w2);
    }
    else if (cc_isinside(p, v2, w1, w0)) {
      name[len++] = '2';
      copy_vec(v0, v2);
      copy_vec(v1, w1);
      copy_vec(v2, w0);
    }
    else if (cc_isinside(p, w0, w1, w2)) {
      name[len++] ='3';
      copy_vec(v0, w0);
      copy_vec(v1, w1);
      copy_vec(v2, w2);
    }
    else {
      fprintf(stderr, "PANIC\n");
    }
  }
  name[len] = '\0';
  rstat = cc_name2ID(name);
  return rstat;

#ifdef SOMETIMECONSIDER
  if (0){

    /**
    // project xyz onto tangent plane
    **/

    lambda = cc_sqrt3 / (x + y + z);
    px = lambda * x;
    py = lambda * y;
    pz = lambda * z;
    /**
    // Now, (px, py, px) = P is on the tangent plane.
    **/
  }

  if(0){
    double d1, d2, d3;
    d1 = acos(x);
    d2 = acos(y);
    d3 = acos(z);
  }
#endif
}

uint64 cc_radec2ID(double ra, double dec, int depth)
{
  uint64 rstat = 0;
  int startID;
  double x, y , z;
  char name[80];
  int len = 0;

  double v1[3], v2[3], v0[3];
  double w1[3], w2[3], w0[3];
  double p[3];
  double cd = cos(dec * cc_Pr);
  double dtmp;

  p[0] = x = cos(ra * cc_Pr) * cd;
  p[1] = y = sin(ra * cc_Pr) * cd;
  p[2] = z = sin(dec* cc_Pr);

  /**
  // Get the ID of the level0 triangle, and its starting vertices
  **/

  startID = cc_startpane(v0, v1, v2, x, y, z, name);
  len = 2;
  /**
  // Start searching for the children
  **/
  while(depth-- > 0){
    m4_midpoint(v0, v1, w2, dtmp);
    m4_midpoint(v1, v2, w0, dtmp);
    m4_midpoint(v2, v0, w1, dtmp);
    if (cc_isinside(p, v0, w2, w1)) {
      name[len++] = '0';
      copy_vec(v1, w2);
      copy_vec(v2, w1);
    }
    else if (cc_isinside(p, v1, w0, w2)) {
      name[len++] = '1';
      copy_vec(v0, v1);
      copy_vec(v1, w0);
      copy_vec(v2, w2);
    }
    else if (cc_isinside(p, v2, w1, w0)) {
      name[len++] = '2';
      copy_vec(v0, v2);
      copy_vec(v1, w1);
      copy_vec(v2, w0);
    }
    else if (cc_isinside(p, w0, w1, w2)) {
      name[len++] ='3';
      copy_vec(v0, w0);
      copy_vec(v1, w1);
      copy_vec(v2, w2);
    }
  }
  name[len] = '\0';
  rstat = cc_name2ID(name);
  return rstat;

#ifdef SOMETIMECONSIDER
  if (0){

    /**
    // project xyz onto tangent plane
    **/

    lambda = cc_sqrt3 / (x + y + z);
    px = lambda * x;
    py = lambda * y;
    pz = lambda * z;
    /**
    // Now, (px, py, px) = P is on the tangent plane.
    **/
  }

  if(0){
    double d1, d2, d3;
    d1 = acos(x);
    d2 = acos(y);
    d3 = acos(z);
  }
#endif
}


/**
// typedef longlong uint64;
**/
uint64 idByPoint(double x, double y, double z)
{
  uint64 ID = 0;

#ifdef	NEVER
  /**
  // start with the 8 root triangles, find the one which v points to
  // But there is a cheaper way...
  **/
  for(ix=1; ix <=8; ix++) {
    if( (V(0) ^ V(1)) * v < -gEpsilon) continue;
    if( (V(1) ^ V(2)) * v < -gEpsilon) continue;
    if( (V(2) ^ V(0)) * v < -gEpsilon) continue;
    break;
  }
  /* loop through matching child until leaves are reached */
  while(ICHILD(0)!=0) {
    uint64 oldindex = index;
    for(size_t i = 0; i < 4; i++) {
      index = nodes_[oldindex].childID_[i];
      if( (V(0) ^ V(1)) * v < -gEpsilon) continue;
      if( (V(1) ^ V(2)) * v < -gEpsilon) continue;
      if( (V(2) ^ V(0)) * v < -gEpsilon) continue;
      break;
    }
  }
  /* return if we have reached maxlevel */
  if(maxlevel_ == buildlevel_)return N(index).id_;

  /**
  // from now on, continue to build name dynamically.
  // until maxlevel_ levels depth, continue to append the
  // correct index, build the index on the fly.
  **/
  char name[HTMNAMEMAX];
  nameById(N(index).id_,name);
  size_t len = strlen(name);
  SpatialVector v0 = V(0);
  SpatialVector v1 = V(1);
  SpatialVector v2 = V(2);

  size_t level = maxlevel_ - buildlevel_;
  while(level--) {

    SpatialVector w0 = v1 + v2; w0.normalize();
    SpatialVector w1 = v0 + v2; w1.normalize();
    SpatialVector w2 = v1 + v0; w2.normalize();

    if(isInside(v, v0, w2, w1)) {
      name[len++] = '0';
      v1 = w2; v2 = w1;
      continue;
    } else if(isInside(v, v1, w0, w2)) {
      name[len++] = '1';
      v0 = v1; v1 = w0; v2 = w2;
      continue;
    } else if(isInside(v, v2, w1, w0)) {
      name[len++] = '2';
      v0 = v2; v1 = w1; v2 = w0;
      continue;
    } else if(isInside(v, w0, w1, w2)) {
      name[len++] = '3';
      v0 = w0; v1 = w1; v2 = w2;
      continue;
    }
  }
  name[len] = '\0';
  ID = idByName(name);uint64 cc_name2ID(const char *name)
    {
      SpatialVector vec;
      pointById(vec, ID);
      //*
      //cerr << "pointById: ----------------" << endl;
      //vec.show();
      **/
    }    
#endif
  return ID;

}

int cc_isinside(double *p, double *v1, double *v2, double *v3) /* p need not be nromalilzed!!! */
{
  double crossp[3];
  /**
  // if (v1 X v2) . p < epsilon then false 
  // same for v2 X v3 and v3 X v1.
  // else return true..
  **/
  crossp[0] = v1[1] * v2[2] - v2[1] * v1[2];
  crossp[1] = v1[2] * v2[0] - v2[2] * v1[0];
  crossp[2] = v1[0] * v2[1] - v2[0] * v1[1];
  if (p[0] * crossp[0] + p[1] * crossp[1] + p[2] * crossp[2] < -gEpsilon)
    return 0;


  crossp[0] = v2[1] * v3[2] - v3[1] * v2[2];
  crossp[1] = v2[2] * v3[0] - v3[2] * v2[0];
  crossp[2] = v2[0] * v3[1] - v3[0] * v2[1];
  if (p[0] * crossp[0] + p[1] * crossp[1] + p[2] * crossp[2] < -gEpsilon)
    return 0;


  crossp[0] = v3[1] * v1[2] - v1[1] * v3[2];
  crossp[1] = v3[2] * v1[0] - v1[2] * v3[0];
  crossp[2] = v3[0] * v1[1] - v1[0] * v3[1];
  if (p[0] * crossp[0] + p[1] * crossp[1] + p[2] * crossp[2] < -gEpsilon)
    return 0;

  return 1;
}

#ifdef NEVER
void
SpatialVector::updateRaDec() {
  dec_ = asin(z_)/gPr; // easy.
  float64 cd = cos(dec_*gPr);
  if(cd>gEpsilon || cd<-gEpsilon)
    if(y_>gEpsilon || y_<-gEpsilon)
      if (y_ < 0.0)
	ra_ = 360 - acos(x_/cd)/gPr;
      else
	ra_ = acos(x_/cd)/gPr;
    else
      ra_ = (x_ < 0.0 ? 180.0 : 0.0);
  else 
    ra_=0.0;
  okRaDec_ = true;
}
#endif

uint64 cc_name2ID(const char *name){

  uint64 out=0, i;
  size_t siz = 0;

  if(name == 0)              /* null pointer-name */
    return 0;
  if(name[0] != 'N' && name[0] != 'S')  /* invalid name */
    return 0;

  siz = strlen(name);       /* determine string length */
  /* at least size-2 required, don't exceed max */
  if(siz < 2)
    return 0;
  if(siz > HTMNAMEMAX)
    return 0;

  for(i = siz-1; i > 0; i--) {  /* set bits starting from the end */
    if(name[i] > '3' || name[i] < '0') {/* invalid name */
      return 0;
    }
    out += ( (uint64)(name[i]-'0')) << 2*(siz - i -1);
  }

  i = 2;                     /* set first pair of bits, first bit always set */
  if(name[0]=='N') i++;      /* for north set second bit too */
  out += (i << (2*siz - 2) );

  
  /************************
			   // This code may be used later for hashing !
			   if(size==2)out -= 8;
			   else {
			   size -= 2;
			   uint32 offset = 0, level4 = 8;
			   for(i = size; i > 0; i--) { // calculate 4 ^ (level-1), level = size-2
			   offset += level4;
			   level4 *= 4;
			   }
			   out -= level4 - offset;
			   }
  **************************/
  return out;
}

#define HTM_INVALID_ID 1

/*
 * cc_IDlevel is a trusting method (assumes that the id is well formed and
 * valid) that returns the level of the trixel represented by the given
 * htm id
 */
int cc_IDlevel(uint64 htmid)
{
  uint32 size=0, i;

#if defined(_WIN32) 
  uint64 IDHIGHBIT = 1;
  uint64 IDHIGHBIT2= 1;
  IDHIGHBIT = IDHIGHBIT << 63;
  IDHIGHBIT2 = IDHIGHBIT2 << 62;
#endif


  /* determine index of first set bit */
  for(i = 0; i < IDSIZE; i+=2) {
	if ( (htmid << i) & IDHIGHBIT ) break;
	/*
	  if ( (id << i) & IDHIGHBIT2 )  // invalid id
	  return HTM_INVALID_ID;
	  // but we trust you now...
	  */
  }
/*   if(id == 0) */
/*     return HTM_INVALID_ID; */

  size=(IDSIZE-i) >> 1;
  /* Size is the length of the string representing the name of the
     trixel, the level is size - 2
  */
  return size-2;
}
int cc_ID2name(char *name, uint64 id)
{

  uint32 size=0, i;
  int c; /* a spare character; */

#if defined(_WIN32) 
  uint64 IDHIGHBIT = 1;
  uint64 IDHIGHBIT2= 1;
  IDHIGHBIT = IDHIGHBIT << 63;
  IDHIGHBIT2 = IDHIGHBIT2 << 62;
#endif



  /* determine index of first set bit */
  for(i = 0; i < IDSIZE; i+=2) {
	if ( (id << i) & IDHIGHBIT ) break;
    if ( (id << i) & IDHIGHBIT2 )  /* invalid id */
      return HTM_INVALID_ID;
  }
  if(id == 0)
    return HTM_INVALID_ID;

  size=(IDSIZE-i) >> 1;

  /* fill characters starting with the last one */
  for(i = 0; i < size-1; i++) {
    c =  '0' + (int) ((id >> i*2) & (uint32) 3);
    name[size-i-1] = (char ) c;
  }

  /* put in first character */
  if( (id >> (size*2-2)) & 1 ) {
    name[0] = 'N';
  } else {
    name[0] = 'S';
  }
  name[size] = 0; /* end string */

  return 0;
}

int cc_name2Triangle(char *name, double *v0, double *v1, double *v2)
{
  int rstat = 0;
  char *s;
  double w1[3], w2[3], w0[3];
  double dtmp;


  /**
  // Get the top level hemi-demi-semi space
  **/
  int k;
  int anchor_offsets[3];
  k = (int) name[1] - '0';

  if (name[0] == 'S') {
    anchor_offsets[0] = S_indexes[k][0];
    anchor_offsets[1] = S_indexes[k][1];
    anchor_offsets[2] = S_indexes[k][2];
  } else {
    anchor_offsets[0] = N_indexes[k][0];
    anchor_offsets[1] = N_indexes[k][1];
    anchor_offsets[2] = N_indexes[k][2];
  }
  s = name+2;
  copy_vec(v0, anchor[anchor_offsets[0]]);
  copy_vec(v1, anchor[anchor_offsets[1]]);
  copy_vec(v2, anchor[anchor_offsets[2]]);

  while(*s){
    m4_midpoint(v0, v1, w2, dtmp);
    m4_midpoint(v1, v2, w0, dtmp);
    m4_midpoint(v2, v0, w1, dtmp);
    switch(*s) {
    case '0':
      copy_vec(v1, w2);
      copy_vec(v2, w1);
      break;
    case '1':
      copy_vec(v0, v1);
      copy_vec(v1, w0);
      copy_vec(v2, w2);
      break;
    case '2':
      copy_vec(v0, v2);
      copy_vec(v1, w1);
      copy_vec(v2, w0);
      break;
    case '3':
      copy_vec(v0, w0);
      copy_vec(v1, w1);
      copy_vec(v2, w2);
      break;
    }
    s++;
  }
  return rstat;
}


