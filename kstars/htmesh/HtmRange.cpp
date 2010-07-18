#include <iostream> // cout
#include <iomanip>  // setw()
#include <HtmRange.h>

#include <stdio.h>
#include <string.h>

#define INSIDE     1
#define OUTSIDE   -1
#define INTERSECT  0
#define GAP_HISTO_SIZE 10000

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
    InclusionType result, t1, t2;
    Key GH, GL, SH, SL;
    // std::cout << "==========" << std::setw(4) << mid << " is ";

    GH = my_his->findMAX(mid);
    GL = my_los->findMAX(mid);

    if (GH < GL) { // GH < GL
        t1 = InclInside;
        // std::cout << "Inside";
    } else {
        t1 = InclOutside;
        // std::cout << "  no  ";
    }
    // std::cout << "   ";

    SH = my_his->findMIN(mid);
    SL = my_los->findMIN(mid);
    if (SH < SL) { // SH < SL
        t2 = InclInside;
        // std::cout << "Inside";
    } else {
        t2 = InclOutside;
        // std::cout << "  no  ";
    }
    // std::cout << " GH = " << my_his->findMAX(mid) << " GL = " << my_los->findMAX(mid);
    // std::cout << " SH = " << my_his->findMIN(mid) << " SL = " << my_los->findMIN(mid);

    // std::cout << std::endl;
    if (t1 == t2){
        result = t1;
    } else {
        result = (t1 == InclInside ? InclHi : InclLo);
    }

//   if (result == InclOutside){
//     if ((GH + 1 == mid) ||  (SL - 1 == mid)){
//       result = InclAdjacent;
//     }
//   }
    return result;
}

void HtmRange::mergeRange(const Key lo, const Key hi)
{

    int lo_flag = tinside(lo);
    int hi_flag = tinside(hi);

    // delete all nodes (key) in his where lo < key < hi
    // delete all nodes (key) in los where lo < key < hi

    my_his->freeRange(lo, hi);
    my_los->freeRange(lo, hi);
    //  std::cout << "After freeRange" << std::endl;
    // my_los->list(std::cout);
    // my_his->list(std::cout);

    //
    // add if not inside a pre-existing interval
    //
    if (lo_flag == InclHi) {
        //std::cerr << "Do not Insert " << std::setw(20) << lo << " into lo" << std::endl;
    } else if (lo_flag == InclLo ||
               (lo_flag ==  InclOutside)
        ){
        //std::cerr << "Insert        " << std::setw(20) << lo << " into lo" << std::endl;
        my_los->insert(lo, 33);
    }

//   else {
//     std::cerr << "Do not Insert " << std::setw(20) << lo << " into lo (other)" << std::endl;
//   }

    if (hi_flag == InclLo){
        // std::cerr << "Do not Insert " << std::setw(20) << hi << " into hi" << std::endl;
        // my_his->insert(lo, 33);
    }
    else if (hi_flag == InclOutside ||
             hi_flag == InclHi) {
        // std::cerr << "Insert        " << std::setw(20) << hi << " into hi" << std::endl;
        my_his->insert(hi, 33);
    }
//   else {
//     std::cerr << "Insert        " << std::setw(20) << hi << " into hi (other) " << std::endl;
//   }
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

int HtmRange::LOWS = 1;
int HtmRange::HIGHS = 2;
