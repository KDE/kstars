/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class AdaptiveOptics
 * AdaptiveOptics class handles control of INDI AdaptiveOptics devices.
 *
 * @author Jasem Mutlaq
 */
class AdaptiveOptics : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit AdaptiveOptics(GenericDevice *parent) : ConcreteDevice(parent) {}
};
}
