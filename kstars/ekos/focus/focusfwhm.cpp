#include "focusfwhm.h"
#include <ekos_focus_debug.h>

namespace Ekos
{

FocusFWHM::FocusFWHM(Mathematics::RobustStatistics::ScaleCalculation scaleCalc)
{
    m_ScaleCalc = scaleCalc;
}

FocusFWHM::~FocusFWHM()
{
}

// Returns true if two rectangular boxes (b1, b2) overlap.
bool FocusFWHM::boxOverlap(const QPair<int, int> b1Start, const QPair<int, int> b1End, const QPair<int, int> b2Start,
                           const QPair<int, int> b2End)
{
    // Check the "x" coordinate held in the first element of the pair
    if (b1Start.first > b2End.first || b2Start.first > b1End.first)
        return false;

    // Check the "y" coordinate held in the second element of the pair
    if (b1Start.second > b2End.second || b2Start.second > b1End.second)
        return false;

    return true;
}

}  // namespace
