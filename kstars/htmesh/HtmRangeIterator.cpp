#include <HtmRangeIterator.h>

Key HtmRangeIterator::next()
{
    Key key = this->nextval;
    getNext();
    return key;
}

void HtmRangeIterator::getNext()
{
    if (currange[0] <= 0){
        nextval = -1;
        return;
    }
    nextval++;
    if (nextval > currange[1]){
        range->getNext(&currange[0], &currange[1]);
        if (currange[0] <= 0){
            nextval = -1;
            return;
        }
        nextval = currange[0];
    }
    return;
}

bool HtmRangeIterator::hasNext()
{
    return (nextval > 0);
}


// HtmRangeIterator::
