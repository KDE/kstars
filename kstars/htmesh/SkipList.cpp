/*
    File: SkipList.C
    Author: Bruno Grossniklaus, 13.11.97
    Mod:    Gyorgy Fekete, Oct-09-2002
    Version: 1.0
    History:
    Nov-13-1997; Gro; Version 1.0
    Oct-09-2002; JHU; Version 1.1

*/

#include <iostream> // cout
#include <iomanip>  // setw
#include <cstdlib>  // rand(), drand48()

#include "SkipListElement.h"
#include "SkipList.h"

#include <config-htmesh.h>

#ifndef HAVE_DRAND48
double drand48()
{
    double result;
#ifdef _WIN32
    result = static_cast<double>(rand());
    result /= RAND_MAX;
#else
    result = static_cast<double>(random());
    result /= LONG_MAX;
#endif
    return result;
}
#endif /* HAVE_DRAND48 */

////////////////////////////////////////////////////////////////////////////////
// get new element level using given probability
////////////////////////////////////////////////////////////////////////////////
long getNewLevel(long maxLevel, float probability)
{
    long newLevel = 0;
    while ((newLevel < maxLevel - 1) && (drand48() < probability)) // fast hack. fix later
        newLevel++;
    return (newLevel);
}

////////////////////////////////////////////////////////////////////////////////
SkipList::SkipList(float probability) : myProbability(probability)
{
    myHeader = new SkipListElement(); // get memory for header element
    myHeader->setKey(KEY_MAX);
    myLength = 0;
    iter     = myHeader;
}

////////////////////////////////////////////////////////////////////////////////
SkipList::~SkipList()
{
    delete myHeader; // free memory for header element
}

////////////////////////////////////////////////////////////////////////////////
void SkipList::insert(const Key searchKey, const Value value)
{
    int i;
    long newLevel;
    SkipListElement *element;
    SkipListElement *nextElement;
    SkipListElement update(SKIPLIST_MAXLEVEL);

    // scan all levels while key < searchKey
    // starting with header in his level
    element = myHeader;
    for (i = myHeader->getLevel(); i >= 0; i--)
    {
        nextElement = element->getElement(i);
        while ((nextElement != NIL) && (nextElement->getKey() < searchKey))
        {
            element     = nextElement;
            nextElement = element->getElement(i);
        }
        // save level pointer
        update.setElement(i, element);
    }

    // key is < searchKey
    element = element->getElement(0);

    if ((element != NIL) && (element->getKey() == searchKey))
    {
        // key exists. set new value
        element->setValue(value);
    }
    else
    {
        // new key. add to list
        // get new level and fix list level

        // get new level
        newLevel = getNewLevel(SKIPLIST_MAXLEVEL, myProbability);
        if (newLevel > myHeader->getLevel())
        {
            // adjust header level
            for (i = myHeader->getLevel() + 1; i <= newLevel; i++)
            {
                // adjust new pointer of new element
                update.setElement(i, myHeader);
            }
            // set new header level
            myHeader->setLevel(newLevel);
        }
        // make new element [NEW *******]
        myLength++;
        element = new SkipListElement(newLevel, searchKey, value);
        for (i = 0; i <= newLevel; i++) // scan all levels
        {
            // set next pointer of new element
            element->setElement(i, update.getElement(i)->getElement(i));
            update.getElement(i)->setElement(i, element);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// greatest key less than searchKey.. almost completely, but not
// quite entirely unlike a search ...
Key SkipList::findMAX(const Key searchKey) const
{
    int i;
    SkipListElement *element;
    SkipListElement *nextElement;
    Key retKey;

    element = myHeader;
    for (i = myHeader->getLevel(); i >= 0; i--)
    {
        nextElement = element->getElement(i);
        while ((nextElement != NIL) && (nextElement->getKey() < searchKey))
        {
            element     = nextElement;
            nextElement = element->getElement(i);
        }
    }
    // now nextElement is >= searhcKey
    // and element is < searchKey
    // therefore element  key is largest key less than current

    // if this were search:
    // element=element->getElement(0); // skip to >=
    if (element != NIL)
    {
        retKey = element->getKey();
        return retKey == KEY_MAX ? (-KEY_MAX) : retKey;
    }
    else
    {
        return ((Key)KEY_MAX);
    }
}

// smallest greater than searchKey.. almost completely, but not
// quite entirely unlike a search ...
Key SkipList::findMIN(const Key searchKey) const
{
    int i;
    SkipListElement *element(myHeader);
    SkipListElement *nextElement = nullptr;
    Key retKey;

    for (i = myHeader->getLevel(); i >= 0; i--)
    {
        nextElement = element->getElement(i);
        while ((nextElement != NIL) && (nextElement->getKey() <= searchKey))
        {
            element     = nextElement;
            nextElement = element->getElement(i);
        }
    }
    // now nextElement is > searchKey
    // element is <= , make it >
    //
    element = nextElement;
    if (element != NIL)
    {
        retKey = element->getKey();
        return retKey == KEY_MAX ? (-KEY_MAX) : retKey;
    }
    else
    {
        return (Key)KEY_MAX;
    }
}

////////////////////////////////////////////////////////////////////////////////
/* Very similar to free, but frees a range of keys
   from lo to hi, inclusive */
void SkipList::freeRange(const Key loKey, const Key hiKey)
{
    int i;
    SkipListElement *element;
    SkipListElement *nextElement;

    // scan all levels while key < searchKey
    // starting with header in his level
    element = myHeader;
    for (i = myHeader->getLevel(); i >= 0; i--)
    {
        nextElement = element->getElement(i);
        while ((nextElement != NIL) && (nextElement->getKey() < loKey))
        {
            element     = nextElement;
            nextElement = element->getElement(i);
        }
    }
    // key is < loKey
    element = element->getElement(0);

    while ((element != NIL) && (element->getKey() <= hiKey))
    {
        nextElement = element->getElement(0);
        free(element->getKey());
        element = nextElement;
    }
}

////////////////////////////////////////////////////////////////////////////////
void SkipList::free(const Key searchKey)
{
    int i;
    SkipListElement *element;
    SkipListElement *nextElement;
    SkipListElement update(SKIPLIST_MAXLEVEL);

    // scan all levels while key < searchKey
    // starting with header in his level
    element = myHeader;
    for (i = myHeader->getLevel(); i >= 0; i--)
    {
        nextElement = element->getElement(i);
        while ((nextElement != NIL) && (nextElement->getKey() < searchKey))
        {
            element     = nextElement;
            nextElement = element->getElement(i);
        }
        // save level pointer
        update.setElement(i, element);
    }

    // key is < searchKey
    element = element->getElement(0);

    // if key exists
    if ((element != NIL) && (element->getKey() == searchKey))
    {
        for (i = 0; i <= myHeader->getLevel(); i++) // save next pointers
        {
            if (update.getElement(i)->getElement(i) == element)
            {
                update.getElement(i)->setElement(i, element->getElement(i));
            }
        }

        // free memory of element
        delete element;
        myLength--;

        // set new header level
        while ((myHeader->getLevel() > 0) && (myHeader->getElement(myHeader->getLevel()) == NIL))
        {
            myHeader->setLevel(myHeader->getLevel() - 1);
        }
    }
}

//// STATISTICS on skiplist
void SkipList::stat()
{
    int count = 0;
    SkipListElement *element;
    SkipListElement *nextElement;

    element     = myHeader;
    nextElement = element->getElement(0);

    while ((nextElement != NIL))
    {
        count++;
        element     = nextElement;
        nextElement = element->getElement(0);
    }
    std::cout << "Have number of elements ... " << count << std::endl;
    std::cout << "Size  ..................... " << myLength << std::endl;
    {
        int *hist;
        hist = new int[20];
        int i;
        long totalPointers, usedPointers;
        for (i = 0; i < 20; i++)
        {
            hist[i] = 0;
        }
        element     = myHeader;
        count       = 0;
        nextElement = element->getElement(0);
        while ((nextElement != NIL))
        {
            count++;
            hist[nextElement->getLevel()]++;
            element     = nextElement;
            nextElement = element->getElement(0);
        }
        //
        // There are SKIPLIST_MAXLEVEL * count available pointers
        //
        totalPointers = SKIPLIST_MAXLEVEL * count;
        usedPointers  = 0;
        //
        // of these every node that is not at the max level wastes some
        //
        for (i = 0; i < 20; i++)
        {
            if (hist[i] > 0)
                std::cout << std::setw(2) << i << ": " << std::setw(6) << hist[i] << std::endl;
            usedPointers += hist[i] * (1 + i);
        }
        std::cout << "Used  pointers " << usedPointers << std::endl;
        std::cout << "Total pointers " << totalPointers
                  << " efficiency = " << (double)usedPointers / (double)totalPointers << std::endl;
        delete[] hist;
    }
    return;
}
