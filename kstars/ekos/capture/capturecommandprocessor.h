/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ekos/ekos.h"
#include "indi/indiccd.h"

namespace Ekos
{
class CaptureCommandProcessor: public QObject
{
    Q_OBJECT

public:
    CaptureCommandProcessor();

    //////////////////////////////////////////////////////////////////////
    // Rotator commands
    //////////////////////////////////////////////////////////////////////
    // active rotator device
    ISD::GDInterface * activeRotator { nullptr };

    /**
     * @brief Set the rotator's angle
     */
    void setRotatorAngle(double *rawAngle);

    //////////////////////////////////////////////////////////////////////
    // Camera commands
    //////////////////////////////////////////////////////////////////////
    // active camera device
    ISD::CCD * activeCCD { nullptr };
    // active CCD chip
    ISD::CCDChip * activeChip { nullptr };

    /**
     * @brief Set the CCD target temperature
     * @param temp
     */
    void setCCDTemperature(double temp);

    /**
     * @brief Set CCD to batch mode
     * @param enable true iff batch mode
     */
    void enableCCDBatchMode(bool enable);

    //////////////////////////////////////////////////////////////////////
    // Filter wheel commands
    //////////////////////////////////////////////////////////////////////
    // currently active filter wheel device
    ISD::GDInterface * activeFilterWheel { nullptr };

};
}; // namespace
