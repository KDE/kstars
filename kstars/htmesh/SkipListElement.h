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

#include <iosfwd>  // -jbb

#define SKIPLIST_MAXLEVEL 6 // maximum node level
#define OS_DEFAULT_WIDTH 3  // width for keys, values for output
#define OS_INDEX_WIDTH 2    // width for index
#define NIL 0               // invalid pointer

#ifdef _WIN32
#define KEY_MAX _I64_MAX
#else
#define KEY_MAX LONG_LONG_MAX
#endif

typedef int64 Key;            // key type
typedef int Value;          // value type

//-jbb class ostream;
class SkipListElement;

class LINKAGE SkipListElement{
public:
  SkipListElement(long level = 0, Key  key = 0, Value  value = 0);
  ~SkipListElement();

  Key getKey() const {return(myKey);}; // get key of element
  void setKey(Key key) {myKey=key;}; // set key of element
  
  Value getValue() const {return(myValue);}; // get value of element
  void setValue(Value value) {myValue=value;}; // set value of element

  long getLevel() const {return(myLevel);}; // get level of element
  void setLevel(long level);
  static void prmatrix();
  SkipListElement* getElement(long level); // get next element in level
  void setElement(long level, SkipListElement* element); // set next element in level
  
  friend std::ostream& operator<<(std::ostream& os, const SkipListElement& element);

private:
  long myLevel; // level of element
  Key myKey; // key of element
  Value myValue; // value of element
  SkipListElement* myNext[SKIPLIST_MAXLEVEL]; // pointers to next elements
};
#endif // _SkipListElement_H
