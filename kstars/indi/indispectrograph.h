/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Spectrograph
 * Spectrograph class handles control of INDI Spectrograph devices.
 *
 * @author Jasem Mutlaq
 */
class Spectrograph : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Spectrograph(GenericDevice *parent) : ConcreteDevice(parent) {}
};
}
