#ifndef _HTMHANGE_H_
#define _HTMHANGE_H_

#include <SkipList.h>

//-jbb class ostream;

enum InclusionType {
  InclOutside = 0,
  InclInside,
  InclLo,			/* number is on low end of an interval */
  InclHi,			/* number is on high end of an interval */
  InclAdjacentXXX
};
  
class LINKAGE HtmRange {

 public:
  static int HIGHS;
  static int LOWS;
  int getNext(Key &lo, Key &hi);
  int getNext(Key *lo, Key *hi);

  void setSymbolic(bool flag);

  void addRange(const Key lo, const Key hi);
  void mergeRange(const Key lo, const Key hi);
  void defrag();
  void defrag(Key gap);
  void levelto(int level);
  void purge();
  int isIn(Key key);
  int isIn(Key lo, Key hi);
  int isIn(HtmRange & otherRange);
  Key bestgap(Key desiredSize);
  int stats(int desiredSize);
  int nranges();
  void reset();

  void print(int what, std::ostream& os, bool symbolic); // FIX THIS, so caller does not set symbolic here....

  int compare(const HtmRange & other) const;


  HtmRange();
  ~HtmRange(){
    purge();
    delete my_los;
    delete my_his;
  };
  
  friend LINKAGE std::ostream& operator<<(std::ostream& os, const HtmRange& range);

 protected:
  InclusionType tinside(const Key mid) const;
  // const char buffer[256];
  bool symbolicOutput;
 private:

  
  SkipList *my_los;
  SkipList *my_his;
};


#define SKIP_PROB (0.5)

#endif
