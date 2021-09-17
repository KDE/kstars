/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "matr.h"
#include "fitsviewer/fitsdata.h"
#include "indi/indicommon.h"
#include "../guideinterface.h"
#include "guidelog.h"
#include "calibration.h"
#include "calibrationprocess.h"

#include <QFile>
#include <QPointer>
#include <QQueue>
#include <QTime>

#include <memory>

class QVector3D;

class cgmath;
class GuideView;
class Edge;

namespace Ekos
{
class InternalGuider : public GuideInterface
{
        Q_OBJECT

    public:
        InternalGuider();

        bool Connect() override
        {
            return true;
        }
        bool Disconnect() override
        {
            return true;
        }
        bool isConnected() override
        {
            return true;
        }

        bool calibrate() override;
        bool guide() override;
        bool abort() override;
        bool suspend() override;
        bool resume() override;

        bool dither(double pixels) override;
        bool ditherXY(double x, double y);

        bool clearCalibration() override;
        bool restoreCalibration();

        bool reacquire() override;

        bool setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY) override;
        bool setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture,
                             double mountFocalLength) override;

        // Set Star Position
        void setStarPosition(QVector3D &starCenter) override;

        // Select algorithm
        void setSquareAlgorithm(int index);

        // Reticle Parameters
        void setReticleParameters(double x, double y);
        bool getReticleParameters(double *x, double *y);

        // Guide Square Box Size
        void setGuideBoxSize(uint32_t value)
        {
            guideBoxSize = value;
        }

        // Guide View
        void setGuideView(GuideView *guideView);
        // Image Data
        void setImageData(const QSharedPointer<FITSData> &data);

        bool start();

        bool isGuiding(void) const;
        void setInterface(void);

        void setSubFramed(bool enable)
        {
            m_isSubFramed = enable;
        }

        bool useSubFrame();

        const Calibration &getCalibration() const;

        // Select a guide star automatically
        bool selectAutoStar();
        bool selectAutoStarSEPMultistar();
        bool SEPMultiStarEnabled();

        // Manual Dither
        bool processManualDithering();

        void updateGPGParameters();
        void resetGPG() override;

    public slots:
        void setDECSwap(bool enable);

    protected slots:
        void trackingStarSelected(int x, int y);
        void setDitherSettled();

    signals:
        void newPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
        void newPulse(GuideDirection dir, int msecs);
        //void newStarPosition(QVector3D, bool);
        void DESwapChanged(bool enable);

    private:
        // Guiding
        bool processGuiding();

        bool abortDither();

        void reset();

        // Logging
        void fillGuideInfo(GuideLog::GuideInfo *info);

        std::unique_ptr<cgmath> pmath;
        QPointer<GuideView> guideFrame;
        QSharedPointer<FITSData> m_ImageData;
        bool m_isStarted { false };
        bool m_isSubFramed { false };
        bool m_isFirstFrame { false };
        int m_starLostCounter { 0 };

        QFile logFile;
        uint32_t guideBoxSize { 32 };

        GuiderUtils::Vector m_DitherTargetPosition;
        uint8_t m_DitherRetries {0};

        QElapsedTimer reacquireTimer;
        int m_highRMSCounter {0};

        GuiderUtils::Matrix ROT_Z;
        Ekos::GuideState rememberState { GUIDE_IDLE };

        // Progressive Manual Dither
        QQueue<GuiderUtils::Vector> m_ProgressiveDither;

        // How many high RMS pulses before we stop
        static const uint8_t MAX_RMS_THRESHOLD = 10;
        // How many lost stars before we stop
        static const uint8_t MAX_LOST_STAR_THRESHOLD = 5;

        // Maximum pulse time limit for immediate capture. Any pulses longer that this
        // will be delayed until pulse is over
        static const uint16_t MAX_IMMEDIATE_CAPTURE = 250;
        // When to start capture before pulse delay is over
        static const uint16_t PROPAGATION_DELAY = 100;

        // How many 'random' pixels can we move before we have to force direction reversal?
        static const uint8_t MAX_DITHER_TRAVEL = 15;

        QPair<double, double> accumulator;
        GuideLog guideLog;

        void iterateCalibration();
        std::unique_ptr<CalibrationProcess> calibrationProcess;
        double calibrationStartX = 0;
        double calibrationStartY = 0;
};
}
