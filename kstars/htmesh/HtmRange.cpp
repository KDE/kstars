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
    symbolicOutput = false;
}

void HtmRange::setSymbolic(bool flag)
{
    symbolicOutput = flag;
}


int HtmRange::compare(const HtmRange & other) const
{
    int rstat = 0;
    if (my_los->getLength() == other.my_los->getLength()){
        rstat = 1;
    }
    return rstat;
}

int HtmRange::isIn(Key a, Key b)
{
    // If both lo and hi are inside some range, and the range numbers agree...
    //
    Key GH_a, GL_a, SH_a, SL_a;
    Key GH_b, GL_b, SH_b, SL_b;
    Key param[8];
    int i;
    int rstat = 0;
    i = 0;
    param[i++] = GL_a = my_los->findMAX(a);
    param[i++] = GH_a = my_his->findMAX(a);

    param[i++] = SL_a = my_los->findMIN(a);
    param[i++] = SH_a = my_his->findMIN(a);

    param[i++] = GL_b = my_los->findMAX(b);
    param[i++] = GH_b = my_his->findMAX(b);

    param[i++] = SL_b = my_los->findMIN(b);
    param[i++] = SH_b = my_his->findMIN(b);

  // std::cout << "(isIn GL_a GH_a SL_a SH_a GL_b GH_b SL_b SH_b)" << std::endl;
  // std::cout << "(isIn" ;

//   for(i=0; i<8; i++){
//     if (param[i] == KEY_MAX)
//       std::cout << "  MAX";
//     else if (param[i] == -KEY_MAX)
//       std::cout << " -MAX";
//     else
//       std::cout << std::setw(5) << param[i];
//   }
//   std::cout << ")" << std::endl;
//   // 0 is intersect, -1 is out +1 is inside
  //

    if(GH_a < GL_a && GL_b < GH_b){
        rstat = 0;
        // std::cout << " <<<<< X (0), GH_a < GL_a && GL_b < GH_b" << std::endl;
    } else if (GL_b == a && SH_a == b){
        rstat = 1;
        // std::cout << " <<<<< I (1), because SH_a == a and GL_b == b perfect match" << std::endl;
    } else if (GL_b > GL_a) {
        rstat = 0;
        // std::cout << " <<<<< X (0), because GL_b > GL_a" << std::endl;
    } else if (GH_a < GL_a) {
        rstat = 1;
        // std::cout << " <<<<< I (1), because GH_a < GL_a, and none of previous conditions" << std::endl;
    } else if (SL_a == b) {
        rstat = 0;
        // std::cout << " <<<<< X (0), b concides " << std::endl;
    } else {
        rstat = -1;
        // std::cout << " <<<<< O (-1), because none of the above" << std::endl;
    }
    return rstat;
}

int HtmRange::isIn(Key key)
{
    InclusionType incl;
    int rstat = 0;
    incl = tinside(key);
    rstat = (incl != InclOutside);
    return rstat;
}

int HtmRange::isIn(HtmRange & otherRange)
{
    HtmRange &o = otherRange;
    Key a, b;
    int rel, sav_rel;
    /*
     * for each range in otheRange, see if there is an intersection,
     * total incluwsion or total exclusion
     */
    o.reset();			// the builtin lows and highs lists
    //
    // Inside, Outside, Intersect.
    //
    sav_rel = rel = -2;			// nothing

    while((a = o.my_los->getkey()) > 0){
        b = o.my_his->getkey();

        rel = isIn(a, b);
        //
        // decide what type rel is, and keep looking for a
        // different type
        //
        if (sav_rel != -2){ // first time
            switch (rel) {
            case INTERSECT:
                break;
            case OUTSIDE:
            case INSIDE:
                if (sav_rel != rel){
                    // some out, some in, therefore intersect
                    break;
                }
            }
        }
        sav_rel = rel;
        o.my_his->step();
        o.my_los->step();
    }
    std::cout << "RETURNING " << rel << std::endl;
    return rel;
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

void HtmRange::addRange(const Key lo, const Key hi)
{
    my_los->insert(lo, (Value) 0);
    my_his->insert(hi, (Value) 0);
    return;
}

void HtmRange::defrag(Key gap)
{
    Key hi, lo, save_key;
    my_los->reset();
    my_his->reset();

    // compare lo at i+1 to hi at i.
    // so step lo by one before anything
    //
    my_los->step();
    while((lo = my_los->getkey()) >= 0){
        hi = my_his->getkey();
        // std::cout << "compare " << hi << "---" << lo << std::endl;

        // If no gap, then if (hi + 1 == lo) is same as if (hi + 1 + gap  == lo)
        //
        if (hi + (Key) 1 + gap >= lo){
            //
            // merge to intervals, delete the lo and the high
            // is iter pointing to the right thing?
            // need setIterator(key past the one you are about to delete
            //
            // std::cout << "Will delete " << lo << " from lo " << std::endl;
            //
            // Look ahead, so you can set iter to it
            //
            my_los->step();
            save_key = my_los->getkey();
            my_los->free(lo);
            // std::cout << "Deleted " << lo << " from lo " << std::endl;
            if (save_key >= 0) {
                my_los->search(save_key, 1); //resets iter for stepping
            }
            // std::cout << "Look ahead to " << save_key << std::endl << std::endl;

            // std::cout << "Will delete " << hi << " from hi " << std::endl;
            my_his->step();
            save_key = my_his->getkey();
            my_his->free(hi);
            // std::cout << "Deleted " << hi << " from hi " << std::endl;
            if (save_key >= 0){
                my_his->search(save_key, 1); //resets iter for stepping
            }
            // std::cout << "Look ahead to " << save_key << std::endl << std::endl;

        } else {
            my_los->step();
            my_his->step();
        }
    }
    // std::cout << "DONE looping"  << std::endl;
    // my_los->list(std::cout);
    // my_his->list(std::cout);
}

void HtmRange::defrag()
{
    Key hi, lo, save_key;
    my_los->reset();
    my_his->reset();

    // compare lo at i+1 to hi at i.
    // so step lo by one before anything
    //
    my_los->step();
    while((lo = my_los->getkey()) >= 0){
        hi = my_his->getkey();
        // std::cout << "compare " << hi << "---" << lo << std::endl;

        if (hi + 1 == lo){
            //
            // merge to intervals, delete the lo and the high
            // is iter pointing to the right thing?
            // need setIterator(key past the one you are about to delete
            //
            // std::cout << "Will delete " << lo << " from lo " << std::endl;
            //
            // Look ahead, so you can set iter to it
            //
            my_los->step();
            save_key = my_los->getkey();
            my_los->free(lo);
            // std::cout << "Deleted " << lo << " from lo " << std::endl;
            if (save_key >= 0) {
                my_los->search(save_key, 1); //resets iter for stepping
            }
            // std::cout << "Look ahead to " << save_key << std::endl << std::endl;

            // std::cout << "Will delete " << hi << " from hi " << std::endl;
            my_his->step();
            save_key = my_his->getkey();
            my_his->free(hi);
            // std::cout << "Deleted " << hi << " from hi " << std::endl;
            if (save_key >= 0){
                my_his->search(save_key, 1); //resets iter for stepping
            }
            // std::cout << "Look ahead to " << save_key << std::endl << std::endl;

        } else {
            my_los->step();
            my_his->step();
        }
    }
    // std::cout << "DONE looping"  << std::endl;
    // my_los->list(std::cout);
    // my_his->list(std::cout);
}
void HtmRange::levelto(int depth)
{
    return;
}

void HtmRange::purge()
{
    my_los->freeRange(-1, KEY_MAX);
    my_his->freeRange(-1, KEY_MAX);
}

void HtmRange::reset()
{
    my_los->reset();
    my_his->reset();

}
//
// return the number of ranges
//
int HtmRange::nranges()
{
    Key lo, hi;
    int n_ranges;
    n_ranges = 0;
    my_los->reset();
    my_his->reset();

    while((lo = my_los->getkey()) > 0){
        n_ranges++;
        hi = my_his->getkey();
        my_los->step();
        my_his->step();
    }
    return n_ranges;
}

//
// return the smallest gapsize at which rangelist would be smaller than
// desired size
//
Key HtmRange::bestgap(Key desiredSize)
{
    SkipList sortedgaps;
    Key gapsize;
    Key key;
    Value val;

    Key lo, hi, oldhi = 0, del;

    int n_ranges, left_ranges;

    n_ranges = 0;
    my_los->reset();
    my_his->reset();

    while((lo = my_los->getkey()) > 0){
        hi = my_his->getkey();
        n_ranges++;
        if (oldhi > 0){
            del = lo - oldhi - (Key) 1;
            val = sortedgaps.search(del);
            if (val == SKIPLIST_NOT_FOUND) {
                // std::cerr << "Insert " << del << ", " << 1 << std::endl;
                sortedgaps.insert(del, 1); // value is number of times this gapsize appears
            } else {
                // std::cerr << "Adding " << del << ", " << val+1 << std::endl;
                sortedgaps.insert(del, (Value) (val+1));
            }
        }
        oldhi = hi;
        my_los->step();
        my_his->step();
    }
    if (n_ranges <= desiredSize) // no need to do anything else
        return 0;

    // increase gapsize until you find that
    // the if you remove the sum of all
    // gapsizes less than or equal to gapsize
    // leaves you with fewer than the deisred number of ranges
    //
    left_ranges = n_ranges;
    sortedgaps.reset();
    while((key = sortedgaps.getkey()) >= 0){
        gapsize = key;
        val = sortedgaps.getvalue();
        // std::cerr << "Ranges left " << left_ranges;
        left_ranges -= (int) val;
        if (left_ranges <= desiredSize)
            break;
        sortedgaps.step();
    }

    // sortedgaps.list(std::cerr);

    // sortedgaps.freeRange(-1, KEY_MAX);

    sortedgaps.freeAll();
    return gapsize;
}

int HtmRange::stats(int desiredSize)
{
    Key lo, hi;
    Key oldhi = (Key) -1;
    Key del;

    Key max_gap = 0;

    int histo[GAP_HISTO_SIZE], i, huges, n_ranges;
    int cumul[GAP_HISTO_SIZE];
    int bestgap = 0;
    int keeplooking;
    // std::cerr << "STATS=========================" << std::endl;
    for(i=0; i<GAP_HISTO_SIZE; i++){
        cumul[i] = histo[i] = 0;
    }
    n_ranges = 0;
    huges = 0;
    max_gap = 0;
    my_los->reset();
    my_his->reset();

    while((lo = my_los->getkey()) > 0){
        // std::cerr << "Compare lo = "  << lo;
        n_ranges++;
        hi = my_his->getkey();
        if (oldhi > 0){

            del = lo - oldhi - 1;
            if (del < GAP_HISTO_SIZE){
                histo[del] ++;
            } else {
                max_gap = max_gap < del ? del : max_gap;
                huges++;
            }
            // std::cerr << " with hi = " << oldhi << std::endl;
        } else {
            // std::cerr << " with nothing" << std::endl;
        }
        oldhi = hi;
        //
        // Look at histogram of gaps
        //

        my_los->step();
        my_his->step();
    }

    if (n_ranges <= desiredSize){
        // std::cerr << "No work needed, n_ranges is leq desired size: " << n_ranges << " <= " << desiredSize << std::endl;
        return -1;			// Do not do unnecessary work
    }

    keeplooking = 1;
    // std::cerr << "Start looking with n_ranges = " << n_ranges << std::endl;


    // SUBTRACT FROM n_ranges, the number of 0 gaps, because they
    // will me merged automatically
    // VERY IMPORTANT!!!
    //
    n_ranges -= histo[0];
    //
    for(i=0; i<GAP_HISTO_SIZE; i++){
        if(i>0){
            cumul[i] = cumul[i-1];
        }
//     else {
//       std::cerr << std::setw(3) << i << ": " << std::setw(6) << histo[i] ;
//       std::cerr << ", " << std::setw(6) << cumul[i] ;
//       std::cerr << " => " << std::setw(6) << n_ranges - cumul[i];
//       std::cerr << std::endl;
//     }

        if(histo[i] > 0){
            cumul[i] += histo[i];
            std::cerr << std::setw(3) << i << ": " << std::setw(6) << histo[i] ;
            std::cerr << ", " << std::setw(6) << cumul[i] ;
            std::cerr << " => " << std::setw(6) << n_ranges - cumul[i];
            // You can stop looking when desiredSize becomes greater than or
            // equal to the n_ranges - the cumulative ranges removed
            //
            keeplooking = (desiredSize < (n_ranges - cumul[i]));
            if (keeplooking == 0)   std::cerr << "   ****";
            std::cerr << std::endl;
            if (!keeplooking){
                break;
            }
            bestgap = i;
        }
    }
    return bestgap;
}

int HtmRange::getNext(Key &lo, Key &hi)
{
	lo = my_los->getkey();
	if (lo <= (Key) 0){
		hi = lo = (Key) 0;
		return 0;
	}
	hi = my_his->getkey();
	my_his->step();
	my_los->step();
	return 1;
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
