#include "focusfourierpower.h"

namespace Ekos
{

FocusFourierPower::FocusFourierPower(Mathematics::RobustStatistics::ScaleCalculation scaleCalc)
{
    m_ScaleCalc = scaleCalc;
}

FocusFourierPower::~FocusFourierPower()
{
}

}  // namespace
