/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Detector
 * Detector class handles control of INDI Detector devices.
 *
 * @author Jasem Mutlaq
 */
class Detector : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Detector(GenericDevice *parent) : ConcreteDevice(parent) {}
};
}
