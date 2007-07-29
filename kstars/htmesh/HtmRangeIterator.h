#ifndef _HTMHANGEITERATOR_H_
#define _HTMHANGEITERATOR_H_

#include <HtmRange.h>

class HtmRangeIterator {
 public:
  Key next();
  char *nextSymbolic(char *buffer); /* User responsible for managing it */
  bool hasNext();
  HtmRangeIterator(HtmRange *ran) {
    range = ran;
    range->reset();
    range->getNext(&currange[0], &currange[1]);
    nextval = currange[0] - 1;
    getNext();
  }
 protected:
  HtmRange *range;
  void getNext();

 private:
  Key nextval;
  Key currange[2];		/* Low and High */
  HtmRangeIterator() : range(0), nextval(-1) {}

};

#endif

