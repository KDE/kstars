
#include <stdio.h>
#include <math.h>
#include <string.h>

#define IDSIZE 64

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

#define HTMNAMEMAX         32

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
