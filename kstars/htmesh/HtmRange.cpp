#include <HtmRange.h>

HtmRange::HtmRange()
{
    my_los = new SkipList;
    my_his = new SkipList;
}

HtmRange::~HtmRange()
{
    my_los->freeRange(-1, KEY_MAX);
    my_his->freeRange(-1, KEY_MAX);
    delete my_los;
    delete my_his;
}

InclusionType HtmRange::tinside(const Key mid) const
{
    // clearly out, inside, share a bounday, off by one to some boundary
    InclusionType t1, t2;

    Key GH = my_his->findMAX(mid);
    Key GL = my_los->findMAX(mid);

    if (GH < GL)
        t1 = InclInside;
    else
        t1 = InclOutside;

    Key SH = my_his->findMIN(mid);
    Key SL = my_los->findMIN(mid);
    if (SH < SL)
        t2 = InclInside;
    else
        t2 = InclOutside;

    if( t1 == t2 )
        return t1;
    if( t1 == InclInside )
        return InclHi;
    else
        return InclLo;
}

void HtmRange::mergeRange(const Key lo, const Key hi)
{
    int lo_flag = tinside(lo);
    int hi_flag = tinside(hi);

    // delete all nodes (key) in his and los where lo < key < hi
    my_his->freeRange(lo, hi);
    my_los->freeRange(lo, hi);

    // add if not inside a pre-existing interval
    if (lo_flag == InclHi) {
    } else if (lo_flag == InclLo || (lo_flag ==  InclOutside) ) {
        my_los->insert(lo, 33);
    }

    if (hi_flag == InclLo){
    } else if (hi_flag == InclOutside || hi_flag == InclHi) {
        my_his->insert(hi, 33);
    }
}

void HtmRange::reset()
{
    my_los->reset();
    my_his->reset();
}

int HtmRange::getNext(Key *lo, Key *hi)
{
	*lo = my_los->getkey();
	if (*lo <= (Key) 0){
		*hi = *lo = (Key) 0;
		return 0;
	}
	*hi = my_his->getkey();
	my_his->step();
	my_los->step();
	return 1;
}
