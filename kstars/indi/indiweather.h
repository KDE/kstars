/*  INDI Weather
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <memory>
#include <QTimer>

#include "indistd.h"

namespace ISD
{
/**
 * @class Weather
 * Focuser class handles control of INDI Weather devices. It reports overall state and the value of each parameter
 *
 * @author Jasem Mutlaq
 */
class Weather : public DeviceDecorator
{
        Q_OBJECT

    public:
        explicit Weather(GDInterface *iPtr);

        void registerProperty(INDI::Property *prop) override;
        void processSwitch(ISwitchVectorProperty *svp) override;
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;

        DeviceFamily getType() override
        {
            return dType;
        }

        IPState getWeatherStatus();

        uint16_t getUpdatePeriod();

    signals:
        void newStatus(IPState status);
        void ready();

    private:
        IPState m_WeatherStatus { IPS_IDLE };
        std::unique_ptr<QTimer> readyTimer;
};
}
