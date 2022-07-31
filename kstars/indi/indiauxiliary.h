/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Auxiliary
 * Auxiliary class handles control of INDI Auxiliary devices.
 *
 * @author Jasem Mutlaq
 */
class Auxiliary : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Auxiliary(GenericDevice *parent) : ConcreteDevice(parent) {}
};
}
