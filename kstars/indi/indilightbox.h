/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indidustcap.h"
#include "indistd.h"

namespace ISD
{
/**
 * @class LightBox
 * Handles operation of a remotely controlled light box.
 *
 * @author Jasem Mutlaq
 */
class LightBox : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit LightBox(GenericDevice *parent) : ConcreteDevice(parent) {}

        Q_SCRIPTABLE virtual bool isLightOn();

public slots:
    /**
     * @brief SetBrightness Set light box brightness levels if dimmable.
     * @param val Value of brightness level.
     * @return True if operation is successful, false otherwise.
     */
    Q_SCRIPTABLE bool setBrightness(uint16_t val);

    /**
     * @brief SetLightEnabled Turn on/off light
     * @param enable true to turn on, false to turn off
     * @return True if operation is successful, false otherwise.
     */
    Q_SCRIPTABLE bool setLightEnabled(bool enable);
};
}
