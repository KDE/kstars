/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    INDI Guide Interface
*/

#pragma once

#include "indiconcretedevice.h"

/**
 * @class Guide
 * Guide is a special class that handles ST4 commands. Since ST4 functionality can be part of a stand alone ST4 device,
 * or as part of a larger device as CCD or Telescope, it is handled separately to enable one ST4 device regardless of the parent device type.
 *
 *  ST4 is a hardware port dedicated to sending guiding correction pulses to the mount.
 *
 * @author Jasem Mutlaq
 */
namespace ISD
{
class Guider : public ConcreteDevice
{
    public:
        Guider(GenericDevice *parent);

        bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
        bool doPulse(GuideDirection dir, int msecs);
        void setDECSwap(bool enable);

    private:
        bool swapDEC { false };
};

}
