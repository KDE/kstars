/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indicommon.h"
#include "../guideinterface.h"
#include "calibration.h"

#include <QFile>
#include <QPointer>
#include <QQueue>
#include <QTime>

#include <memory>

class QVector3D;
class GuideView;
class Edge;
class GuideLog;

namespace Ekos
{
class CalibrationProcess
{
    public:
        enum CalibrationStage
        {
            CAL_IDLE,
            CAL_ERROR,
            CAL_CAPTURE_IMAGE,
            CAL_SELECT_STAR,
            CAL_START,
            CAL_RA_INC,
            CAL_RA_DEC,
            CAL_DEC_INC,
            CAL_DEC_DEC,
            CAL_BACKLASH
        };
        enum CalibrationType
        {
            CAL_NONE,
            CAL_RA_AUTO,
            CAL_RA_DEC_AUTO
        };

        CalibrationProcess(double startX, double startY, bool raOnly);
        ~CalibrationProcess() {}

        void useCalibration(Calibration *calibrationPtr);
        bool inProgress() const;

        void startup();
        void setGuideLog(GuideLog *guideLogPtr);

        void iterate(double x, double y);

        // Return values from each iteration.
        void getCalibrationUpdate(
            GuideInterface::CalibrationUpdateType *type,
            QString *message, double *x, double *y) const;
        void getPulse(GuideDirection *dir, int *msecs) const;
        QString getLogStatus() const;
        Ekos::GuideState getStatus() const;

    private:
        void initializeIteration();

        // Methods corresponding to each calibration state.
        void startState();
        void raOutState(double cur_x, double cur_y);
        void raInState(double cur_x, double cur_y);
        void decBacklashState(double cur_x, double cur_y);
        void decOutState(double cur_x, double cur_y);
        void decInState(double cur_x, double cur_y);

        // Setup the return values from each iteration.
        void addCalibrationUpdate(GuideInterface::CalibrationUpdateType type,
                                  QString message, double x = 0, double y = 0);

        void addPulse(GuideDirection dir, int msecs);
        void addLogStatus(const QString &status);
        void addStatus(Ekos::GuideState s);

        // calibration parameters
        int maximumSteps { 5 };
        int turn_back_time { 0 };
        int ra_iterations { 0 };
        int dec_iterations { 0 };
        int backlash_iterations { 0 };
        int last_pulse { 0 };
        int ra_total_pulse { 0 };
        int de_total_pulse { 0 };
        uint8_t backlash { 0 };

        // calibration coordinates
        double start_x1 { 0 };
        double start_y1 { 0 };
        double end_x1 { 0 };
        double end_y1 { 0 };
        double start_x2 { 0 };
        double start_y2 { 0 };
        double end_x2 { 0 };
        double end_y2 { 0 };
        double last_x { 0 };
        double last_y { 0 };
        double ra_distance {0};
        double de_distance {0};
        double start_backlash_x { 0 };
        double start_backlash_y { 0 };

        CalibrationStage calibrationStage { CAL_IDLE };
        CalibrationType calibrationType;

        Calibration *calibration = nullptr;
        Calibration tempCalibration;
        bool raOnly = false;
        GuideLog *guideLog = nullptr;

        // Return values from each iteration.
        // Most of these result in emits in internalguider.cpp
        bool axisCalibrationComplete = false;
        QString logString;
        GuideInterface::CalibrationUpdateType updateType;
        QString calibrationUpdate;
        double updateX, updateY;
        Ekos::GuideState status;
        GuideDirection pulseDirection;
        int pulseMsecs = 0;
};
}
