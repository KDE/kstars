#ifndef _SkipListElement_H
#define _SkipListElement_H

/*
  File: SkipListElement.h
  Interface for skip list elements
  See William Pugh's paper: 
    Skip Lists: A Probabilistic Alternative to Balanced Trees
  Author: Bruno Grossniklaus, 13.11.97
  Version: 1.0
  History:
  13.11.97; Gro; Version 1.0
*/

#include <SpatialGeneral.h>

#define SKIPLIST_MAXLEVEL 6 // maximum node level
#define NIL 0               // invalid pointer

#ifdef _WIN32
#define KEY_MAX _I64_MAX
#else
#  if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || ( defined(__SUNPRO_CC) && defined(__sun) )
#    define KEY_MAX LLONG_MAX
#  else
#    define KEY_MAX LONG_LONG_MAX
#  endif
#endif

typedef int64 Key;    // key type
typedef int Value;    // value type

class SkipListElement;

class LINKAGE SkipListElement{
public:
    SkipListElement(long level = 0, Key  key = 0, Value  value = 0);

    /** get key of element */
    Key getKey() const   { return myKey; };
    /** set key of element */
    void setKey(Key key) { myKey = key; };
  
    /** get value of element */
    Value getValue() const     { return myValue;}
    /** set value of element */
    void setValue(Value value) { myValue = value;}

    /** get level of element */
    long getLevel() const {return(myLevel);};
    /** Set level of element */
    void setLevel(long level);

    /** get next element in level */
    SkipListElement* getElement(long level); 
    /** set next element in level */
    void setElement(long level, SkipListElement* element);
  
private:
    long myLevel;  // level of element
    Key myKey;     // key of element
    Value myValue; // value of element
    SkipListElement* myNext[SKIPLIST_MAXLEVEL]; // pointers to next elements
};
#endif // _SkipListElement_H
