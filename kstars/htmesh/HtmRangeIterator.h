#ifndef _HTMHANGEITERATOR_H_
#define _HTMHANGEITERATOR_H_

#include <HtmRange.h>

class HtmRangeIterator {
public:
    Key next();
    bool hasNext();
    HtmRangeIterator(HtmRange *ran)
    {
        range = ran;
        range->reset();
        range->getNext(&currange[0], &currange[1]);
        nextval = currange[0] - 1;
        getNext();
    }
private:
    HtmRange *range;
    void getNext();

    Key nextval;
    Key currange[2];
};

#endif

