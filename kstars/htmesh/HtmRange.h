#ifndef _HTMHANGE_H_
#define _HTMHANGE_H_

#include <SkipList.h>

enum InclusionType
{
    InclOutside = 0,
    InclInside,
    InclLo, /* number is on low end of an interval */
    InclHi, /* number is on high end of an interval */
    InclAdjacentXXX
};

class LINKAGE HtmRange
{
  public:
    HtmRange();
    ~HtmRange();

    int getNext(Key *lo, Key *hi);

    void mergeRange(const Key lo, const Key hi);
    void reset();

  protected:
    InclusionType tinside(const Key mid) const;

  private:
    SkipList *my_los;
    SkipList *my_his;
};

#endif
