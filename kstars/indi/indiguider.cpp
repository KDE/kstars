/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    INDI Guide Interface
*/

#include "indiguider.h"

namespace ISD
{

Guider::Guider(GenericDevice *parent) : ConcreteDevice(parent)
{
}

void Guider::setDECSwap(bool enable)
{
    swapDEC = enable;
}

bool Guider::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    bool raOK  = doPulse(ra_dir, ra_msecs);
    bool decOK = doPulse(dec_dir, dec_msecs);
    return (raOK && decOK);
}

bool Guider::doPulse(GuideDirection dir, int msecs)
{
    auto raPulse  = getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INDI::PropertyView<INumber> *npulse   = nullptr;
    INDI::WidgetView<INumber> *dirPulse   = nullptr;

    if (!raPulse || !decPulse)
        return false;

    if (dir == RA_INC_DIR || dir == RA_DEC_DIR)
    {
        raPulse->at(0)->setValue(0);
        raPulse->at(1)->setValue(0);
    }
    else
    {
        decPulse->at(0)->setValue(0);
        decPulse->at(1)->setValue(0);
    }

    switch (dir)
    {
        case RA_INC_DIR:
            npulse = raPulse;
            dirPulse = npulse->findWidgetByName("TIMED_GUIDE_W");
            break;

        case RA_DEC_DIR:
            npulse = raPulse;
            dirPulse = npulse->findWidgetByName("TIMED_GUIDE_E");
            break;

        case DEC_INC_DIR:
            npulse = decPulse;
            dirPulse = npulse->findWidgetByName(swapDEC ? "TIMED_GUIDE_S" : "TIMED_GUIDE_N");
            break;

        case DEC_DEC_DIR:
            npulse = decPulse;
            dirPulse = npulse->findWidgetByName(swapDEC ? "TIMED_GUIDE_N" : "TIMED_GUIDE_S");
            break;

        default:
            return false;
    }

    if (!dirPulse)
        return false;

    dirPulse->setValue(msecs);

    sendNewProperty(npulse);

    return true;
}
}
