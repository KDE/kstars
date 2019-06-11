/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "observatorydomemodel.h"
#include "observatoryweathermodel.h"

#include <QObject>

namespace Ekos
{

struct ObservatoryStatusControl
{
    bool useDome, useShutter, useWeather;
};

class ObservatoryModel : public QObject
{

    Q_OBJECT

public:
    ObservatoryModel();

    ObservatoryDomeModel *getDomeModel() { return mDomeModel; }
    ObservatoryWeatherModel *getWeatherModel() { return mWeatherModel; }

    void setDomeModel(ObservatoryDomeModel *model);
    void setWeatherModel(ObservatoryWeatherModel *model);

    /**
     * @brief Retrieve the settings that define, from which states the
     * "ready" state of the observatory is derived from.
     */
    ObservatoryStatusControl statusControl() { return mStatusControl; }
    void setStatusControl(ObservatoryStatusControl control);

    /**
     * @brief Is the observatory ready? This depends upon the states of the weather,
     * dome etc and upon whether these settings are relevant (see status control).
     */
    bool isReady();

public slots:
    // call this slot in case that the weather or dome status has changed
    void updateStatus();

    /**
     * @brief Depending on the status control settings execute everything so
     * that the status reaches the state "READY".
     */
    void makeReady();

signals:
    /**
     * @brief Signal a new observatory status
     * @param isReady
     */
    void newStatus(bool isReady);


private:
    ObservatoryStatusControl mStatusControl;

    ObservatoryDomeModel *mDomeModel = nullptr;
    ObservatoryWeatherModel *mWeatherModel = nullptr;

};

}
