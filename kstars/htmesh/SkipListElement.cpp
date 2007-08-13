/*
  File: SkipListElement.C
  Author: Bruno Grossniklaus, 13.11.97
  Version: 1.0
  History:
  13.11.97; Gro; Version 1.0
*/

#include <iostream> // cout
#include <iomanip> // setw()
#include <stdlib.h> // rand(), drand48()
#include <limits.h> // INT_MAX

#include "SkipListElement.h"

////////////////////////////////////////////////////////////////////////////////
SkipListElement::SkipListElement(long level, Key key, Value value) :
  myLevel(level),
  myKey(key),
  myValue(value)
{
  int i;
  // init pointers to next elements

  for(i=0; i<SKIPLIST_MAXLEVEL; i++) {
    myNext[i]=NIL;
  }
}

////////////////////////////////////////////////////////////////////////////////
SkipListElement::~SkipListElement()
{
}

////////////////////////////////////////////////////////////////////////////////
SkipListElement* SkipListElement::getElement(long level)
{
  if (level > myLevel) {
    std::cerr << "Error in :" << "SkipListElement::getElement() level:";
    std::cerr << level << ", my level:" << myLevel << ", max level: " << SKIPLIST_MAXLEVEL << std::endl;
    return(this);
  } else {
    return(myNext[level]);
  }
}

////////////////////////////////////////////////////////////////////////////////
void SkipListElement::setElement(long level, SkipListElement* element)
{
  if (level > myLevel) {
    std::cerr << "Error in :" << "SkipListElement::setElement() level:";
    std::cerr << level << ", my level:" << myLevel << ", max level: " << SKIPLIST_MAXLEVEL << std::endl;
  } else {
    myNext[level]=element;
  }
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const SkipListElement& element)
{
  int i;
  int64 x = 34;
  os << x;
  // print element
  os << "(Ske " ;
  if ( element.myKey == -INT_MAX) 
    os << "(k "  << std::setw(OS_DEFAULT_WIDTH) << "-MAX)"; 
  else
    os << "(k "  << std::setw(OS_DEFAULT_WIDTH) << element.myKey << ")"; 
  os << "(v " << std::setw(OS_DEFAULT_WIDTH) << element.myValue << ")";
  os << "(l " << std::setw(OS_DEFAULT_WIDTH) << element.myLevel << ")";;
  os << " PTRs: ";

  // print pointer (keys) to next
  // for (i=0; i<element.myLevel; i++) {

  for (i=0; i<SKIPLIST_MAXLEVEL; i++) {
    if (element.myNext[i] != NIL) {
      os <<"("<< std::setw(OS_INDEX_WIDTH) << i << ": " << std::setw(OS_DEFAULT_WIDTH) << element.myNext[i]->myKey << ")";
    } else {
      os  <<"("<< std::setw(OS_INDEX_WIDTH) << i << ": " << std::setw(OS_DEFAULT_WIDTH) << "NIL" << ")";
    }
  }
  os << ")";
  return(os);
}

static long xMatrix[SKIPLIST_MAXLEVEL][SKIPLIST_MAXLEVEL] = {{0}};;
// static long xFlag = 1; 

void SkipListElement::prmatrix()
{
    for(int i=0; i<SKIPLIST_MAXLEVEL; i++){
      for(int j=0; j<SKIPLIST_MAXLEVEL; j++){
	std::cout <<std::setw(4) << xMatrix[i][j];
      }
      std::cout << "; " << std::endl;
    }
}


void SkipListElement::setLevel(long level)
{
//    if(xFlag){ // do this once only
//      xFlag = 0;
//      // SkipListElement::prmatrix();
//    }
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//   if (myLevel > level){
//     std::cerr << "Panic! level decreasing!" << std::endl;
//   }

  xMatrix[myLevel][level]++;
  myLevel=level;
  
    
}; // set level of element
