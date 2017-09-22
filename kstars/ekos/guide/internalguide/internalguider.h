/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "matr.h"
#include "indi/indicommon.h"
#include "../guideinterface.h"

#include <QFile>
#include <QPointer>

#include <memory>

class QVector3D;

class cgmath;
class FITSView;
class Edge;

namespace Ekos
{
class InternalGuider : public GuideInterface
{
    Q_OBJECT

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
        CAL_DEC_DEC
    };
    enum CalibrationType
    {
        CAL_NONE,
        CAL_RA_AUTO,
        CAL_RA_DEC_AUTO
    };

    InternalGuider();

    bool Connect() override { return true; }
    bool Disconnect() override { return true; }
    bool isConnected() override { return true; }

    bool calibrate() override;
    bool guide() override;
    bool abort() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;

    bool setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY) override;
    bool setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture,
                         double mountFocalLength) override;

    // Set Star Position
    void setStarPosition(QVector3D &starCenter) override;

    // Select algorithm
    void setSquareAlgorithm(int index);

    // Reticle Parameters
    void setReticleParameters(double x, double y, double angle);
    bool getReticleParameters(double *x, double *y, double *angle);

    // Guide View
    void setGuideView(FITSView *guideView);

    // Region Axis
    void setRegionAxis(uint32_t value);

    bool start();

    bool isGuiding(void) const;
    void setAO(bool enable);
    void setInterface(void);
    bool isRapidGuide() { return m_useRapidGuide; }

    double getAOLimit();
    void setSubFramed(bool enable) { m_isSubFramed = enable; }
    void setGuideOptions(const QString &algorithm, bool useSubFrame, bool useRapidGuide);

    QString getAlgorithm();
    bool useSubFrame();
    bool useRapidGuide();

    bool isImageGuideEnabled() const;
    void setImageGuideEnabled(bool value);

    QList<Edge *> getGuideStars();

  public slots:
    void setDECSwap(bool enable);

  protected slots:
    void trackingStarSelected(int x, int y);
    void setDitherSettled();

  signals:
    void newPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
    void newPulse(GuideDirection dir, int msecs);
    void DESwapChanged(bool enable);

  private:
    // Calibration
    void calibrateRADECRecticle(bool ra_only); // 1 or 2-axis calibration
    void processCalibration();

    // Guiding
    bool processGuiding();

    // Image Guiding
    bool processImageGuiding();

    void reset();

    std::unique_ptr<cgmath> pmath;
    QPointer<FITSView> guideFrame;
    bool m_isStarted { false };
    bool m_isSubFramed { false };
    bool isFirstFrame { false };
    bool first_subframe { false };
    bool imageGuideEnabled { false };
    int m_lostStarTries { 0 };
    bool m_useRapidGuide { false };
    QFile logFile;
    int auto_drift_time { 5 };
    int turn_back_time { 0 };
    double start_x1 { 0 };
    double start_y1 { 0 };
    double end_x1 { 0 };
    double end_y1 { 0 };
    double start_x2 { 0 };
    double start_y2 { 0 };
    double end_x2 { 0 };
    double end_y2 { 0 };
    int iterations { 0 };
    int dec_iterations { 0 };
    double phi { 0 };
    Matrix ROT_Z;
    CalibrationStage calibrationStage { CAL_IDLE };
    CalibrationType calibrationType;
};
}
