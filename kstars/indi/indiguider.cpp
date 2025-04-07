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
    INDI::PropertyNumber raPulse  = getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INDI::PropertyNumber decPulse = getNumber("TELESCOPE_TIMED_GUIDE_NS");

    if (!raPulse || !decPulse)
        return false;

    if (dir == RA_INC_DIR || dir == RA_DEC_DIR)
    {
        raPulse[0].setValue(0);
        raPulse[1].setValue(0);
    }
    else
    {
        decPulse[0].setValue(0);
        decPulse[1].setValue(0);
    }

    switch (dir)
    {
        case RA_INC_DIR: // RA is positive E of N!)
            raPulse.findWidgetByName("TIMED_GUIDE_E")->setValue(msecs);
            sendNewProperty(raPulse);
            break;

        case RA_DEC_DIR:
            raPulse.findWidgetByName("TIMED_GUIDE_W")->setValue(msecs);
            sendNewProperty(raPulse);
            break;

        case DEC_INC_DIR:
            decPulse.findWidgetByName(swapDEC ? "TIMED_GUIDE_S" : "TIMED_GUIDE_N")->setValue(msecs);
            sendNewProperty(decPulse);
            break;

        case DEC_DEC_DIR:
            decPulse.findWidgetByName(swapDEC ? "TIMED_GUIDE_N" : "TIMED_GUIDE_S")->setValue(msecs);
            sendNewProperty(decPulse);
            break;

        default:
            return false;
    }

    return true;
}
}
