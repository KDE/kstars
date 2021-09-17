/*
    File: SkipListElement.C
    Author: Bruno Grossniklaus, 13.11.97
    Version: 1.0
    History:
    13.11.97; Gro; Version 1.0
*/

#include <iostream> // cout

#include "SkipListElement.h"

////////////////////////////////////////////////////////////////////////////////
SkipListElement::SkipListElement(long level, Key key, Value value) : myLevel(level), myKey(key), myValue(value)
{
    for (int i = 0; i < SKIPLIST_MAXLEVEL; i++)
        myNext[i] = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
SkipListElement *SkipListElement::getElement(long level)
{
    if (level > myLevel)
    {
        std::cerr << "Error in :"
                  << "SkipListElement::getElement() level:";
        std::cerr << level << ", my level:" << myLevel << ", max level: " << SKIPLIST_MAXLEVEL << std::endl;
        return (this);
    }
    else
    {
        return (myNext[level]);
    }
}

////////////////////////////////////////////////////////////////////////////////
void SkipListElement::setElement(long level, SkipListElement *element)
{
    if (level > myLevel)
    {
        std::cerr << "Error in :"
                  << "SkipListElement::setElement() level:";
        std::cerr << level << ", my level:" << myLevel << ", max level: " << SKIPLIST_MAXLEVEL << std::endl;
    }
    else
    {
        myNext[level] = element;
    }
}
