
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
