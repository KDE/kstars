#ifndef _SkipList_H
#define _SkipList_H

/*
  SkipList.h

  See: Pugh, William,  "Skip Lists: A Probabilistic Alternative to Balanced Trees"

*/
#include <limits.h> // INT_MAX
#include <SkipListElement.h>

#define SKIPLIST_NOT_FOUND -1

class SkipListElement;

class LINKAGE SkipList{
public:
    SkipList(float probability = 0.5);
    ~SkipList();

    /// insert new element
    void insert(const Key searchKey, const Value value);
    /// search element with key NGT searchKey
    Key findMAX(const Key searchKey) const; 
    /// search element with key NLT searchKey
    Key findMIN(const Key searchKey) const; 
    /* ITERATOR SUPPRT */
    void reset() {iter = myHeader->getElement(0);}
    int step() {
        iter = iter->getElement(0); return (iter != NIL);
    }

    Key getkey() {
        if (iter != NIL)
            return iter->getKey();
        else
            return (Key) -1;
    }

    Value getvalue() {
        if (iter != NIL)
            return iter->getValue();
        else
            return (Value) -1;
    }

    /// free element with key
    void free(const Key searchKey); 
    void freeRange(const Key loKey, const Key hiKey);

    /// statistics;
    void stat(); 

private:
    float myProbability;
    /// the header (first) list element
    SkipListElement* myHeader;
    SkipListElement* iter;
    long myLength;
};

#endif // _SkipList_H
