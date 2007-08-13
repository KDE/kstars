#include <HtmRangeIterator.h>

extern "C" {
  // uint64 cc_name2ID(const char *name);
  int cc_ID2name(char *name, uint64 id);
};

Key HtmRangeIterator::next()
{
  Key key = this->nextval;
  getNext();
  return (Key) key;
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

char * HtmRangeIterator::nextSymbolic(char *buffer)
{
  Key key = this->next();
  if (key <= 0) {
    buffer[0] = '\0';
  } else {
    cc_ID2name(buffer, key);
  }
  return buffer;
}
bool HtmRangeIterator::hasNext()
{
  return (nextval > 0);
}


// HtmRangeIterator::
