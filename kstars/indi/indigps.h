/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Filter
 * Filter class handles control of INDI Filter devices.
 *
 * @author Jasem Mutlaq
 */
class GPS : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit GPS(GenericDevice *parent) : ConcreteDevice(parent) {}
};
}
