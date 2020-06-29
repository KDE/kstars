/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "../auxiliary/dome.h"
#include "observatoryweathermodel.h"

#include <QObject>


namespace Ekos
{

class ObservatoryDomeModel: public QObject
{
        Q_OBJECT

    public:
        ObservatoryDomeModel() = default;

        void initModel(Dome *dome);
        bool isActive() {return initialized;}

        ISD::Dome::Status status();
        ISD::Dome::ShutterStatus shutterStatus();

        // proxies to the underlying dome object
        bool canPark()
        {
            return (domeInterface != nullptr && domeInterface->canPark());
        }
        void park();
        void unpark();
        ISD::ParkStatus parkStatus();

        double azimuthPosition()
        {
            return domeInterface->azimuthPosition();
        }
        void setAzimuthPosition(double position)
        {
            domeInterface->setAzimuthPosition(position);
        }

        bool canAbsoluteMove()
        {
            return (domeInterface != nullptr && domeInterface->canAbsoluteMove());
        }

        void setRelativePosition(double position)
        {
            domeInterface->setRelativePosition(position);
        }

        bool canRelativeMove()
        {
            return (domeInterface != nullptr && domeInterface->canRelativeMove());
        }

        bool isRolloffRoof()
        {
            return (domeInterface != nullptr && domeInterface->isRolloffRoof());
        }

        bool isAutoSync()
        {
            return (domeInterface != nullptr && domeInterface->isAutoSync());
        }

        void setAutoSync(bool activate);

        void abort();

        bool hasShutter()
        {
            return (domeInterface != nullptr && domeInterface->hasShutter());
        }
        void openShutter();
        void closeShutter();

        bool moveDome(bool moveCW, bool start);

    public slots:
        void execute(WeatherActions actions);


    private:
        Dome *domeInterface;
        bool initialized = false;

    signals:
        void newStatus(ISD::Dome::Status state);
        void newParkStatus(ISD::ParkStatus status);
        void newShutterStatus(ISD::Dome::ShutterStatus status);
        void newAutoSyncStatus(bool enabled);
        void azimuthPositionChanged(double position);
        void ready();
        void disconnected();
        void newLog(const QString &text);
};

}
