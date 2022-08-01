/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Correlator
 * Correlator class handles control of INDI Correlator devices.
 *
 * @author Jasem Mutlaq
 */
class Correlator : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Correlator(GenericDevice *parent) : ConcreteDevice(parent) {}
};
}
