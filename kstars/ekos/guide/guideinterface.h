/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/ekos.h"
#include "indi/indimount.h"
#include <QObject>
#include <QVector3D>

#include <cstdint>

class QString;

namespace Ekos
{
/**
 * @class GuideInterface
 * @short Interface skeleton for implementation of different guiding applications and/or routines
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class GuideInterface : public QObject
{
        Q_OBJECT

    public:
        GuideInterface() = default;
        virtual ~GuideInterface() override = default;

        virtual bool Connect()     = 0;
        virtual bool Disconnect()  = 0;
        virtual bool isConnected() = 0;

        virtual bool calibrate()           = 0;
        virtual bool guide()               = 0;
        virtual bool suspend()             = 0;
        virtual bool resume()              = 0;
        virtual bool abort()               = 0;
        virtual bool dither(double pixels) = 0;
        virtual bool clearCalibration()    = 0;
        virtual bool reacquire()
        {
            return false;
        }

        virtual bool setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture,
                                     double mountFocalLength);
        virtual bool getGuiderParams(double *ccdPixelSizeX, double *ccdPixelSizeY, double *mountAperture,
                                     double *mountFocalLength);

        virtual bool setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY);
        virtual bool getFrameParams(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h, uint16_t *binX, uint16_t *binY);

        virtual void setStarPosition(QVector3D &starCenter);

        virtual void setMountCoords(const SkyPoint &position, ISD::Mount::PierSide side);
        virtual void setPierSide(ISD::Mount::PierSide newSide);
        virtual void resetGPG() {};

        enum CalibrationUpdateType
        {
            RA_IN,
            RA_OUT,
            RA_OUT_OK,
            BACKLASH,
            DEC_IN,
            DEC_OUT,
            DEC_OUT_OK,
            CALIBRATION_MESSAGE_ONLY
        };

    Q_SIGNALS:
        void newLog(const QString &);
        void newStatus(Ekos::GuideState);
        void newAxisDelta(double delta_ra, double delta_dec);
        void newAxisSigma(double sigma_ra, double sigma_dec);
        void newAxisPulse(double pulse_ra, double pulse_dec);
        void newStarPosition(const QVector3D &newCenter, bool updateNow);
        void newStarPixmap(QPixmap &);
        void newSNR(double snr);
        void calibrationUpdate(CalibrationUpdateType type, const QString &message = QString(""), double x = 0, double y = 0);
        void frameCaptureRequested();
        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);
        void guideEquipmentUpdated();
        void guideInfo(const QString &);
        void abortExposure();

    protected:
        Ekos::GuideState state { GUIDE_IDLE };
        double ccdPixelSizeX { 0 };
        double ccdPixelSizeY { 0 };
        double mountAperture { 0 };
        double mountFocalLength { 0 };
        uint16_t subX { 0 };
        uint16_t subY { 0 };
        uint16_t subW { 0 };
        uint16_t subH { 0 };
        uint16_t subBinX { 1 };
        uint16_t subBinY { 1 };

        // Recent mount position.
        dms mountRA, mountDEC, mountAzimuth, mountAltitude;
        ISD::Mount::PierSide pierSide { ISD::Mount::PIER_UNKNOWN };
};

// StartCaptureAfterPulses  : single-capture mode — request the next exposure once the pulse completes.
// DontCaptureAfterPulses   : streaming correction pulse — frames arrive continuously; gate the stream
//                            while the mount settles so the next measured frame isn't taken mid-move.
// DarkGuidePulse           : a between-frame prediction pulse (GPG/AI dark guiding). Like
//                            DontCaptureAfterPulses it requests no capture, but it must NOT gate the
//                            stream — its whole purpose is to run between real frames, so gating it
//                            would drop the very measurements the filter needs (frame starvation).
enum CaptureAfterPulses {StartCaptureAfterPulses, DontCaptureAfterPulses, DarkGuidePulse};

}
