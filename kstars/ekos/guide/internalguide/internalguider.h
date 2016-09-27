/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef INTERNALGUIDER_H
#define INTERNALGUIDER_H

#include <QFile>
#include <QPixmap>

#include "fitsviewer/fitsview.h"

#include "matr.h"
#include "../guideinterface.h"

class cgmath;

namespace Ekos
{

class InternalGuider : public GuideInterface
{

public:

    enum CalibrationStage { CAL_CAPTURE_IMAGE, CAL_SELECT_STAR, CAL_FINISH, CAL_ERROR, CAL_START, CAL_RA_INC, CAL_RA_DEC, CAL_DEC_INC, CAL_DEC_DEC };
    enum CalibrationType { CAL_NONE, CAL_MANUAL, CAL_RA_AUTO, CAL_RA_DEC_AUTO };

    InternalGuider();
    ~InternalGuider();

    bool Connect() override { return true; }
    bool Disconnect() override { return true; }

    bool calibrate() override;
    bool guide() override;
    bool stop() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;

    bool setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture, double mountFocalLength) override;

    void setSquareAlgorithm( int index );

    // Reticle Parameters
    void setReticleParameters(double x, double y, double angle);
    bool getReticleParameters(double *x, double *y, double *angle);



    /// IMPORTED CHECK THEM ALL

    bool start();
    bool abort(bool silence=false);
    void setHalfRefreshRate( bool is_half );
    bool isGuiding( void ) const;
    void setAO(bool enable);
    void setInterface( void );    
    void setReady(bool enable) { m_isReady = enable;}    
    bool isRapidGuide() { return m_useRapidGuide;}

    double getAOLimit();
    void setSubFramed(bool enable) { m_isSubFramed = enable;}
    void setGuideOptions(const QString & algorithm, bool useSubFrame, bool useRapidGuide);

    QString getAlgorithm();
    bool useSubFrame();
    bool useRapidGuide();

public slots:
    void setDECSwap(bool enable);

    // OBSELETE
    void connectPHD2();
    void setPHD2Connected();
    void setPHD2Disconnected();
    // Only called by PHD2
    void toggleExternalGuideStateGUI(Ekos::GuideState state);

protected slots:
    void openCalibrationOptions();
    void openGuideOptions();

    void capture();
    void trackingStarSelected(int x, int y);


signals:
    void newProfilePixmap(QPixmap &);

private:
    cgmath *pmath;
    QPointer<FITSView> guideFrame;
    bool m_isStarted;
    bool m_isReady;
    bool m_isSubFramed;
    bool first_frame, first_subframe;
    bool half_refresh_rate;
    int m_lostStarTries;
    bool m_useRapidGuide;    
    int fx,fy,fw,fh;
    double ret_x, ret_y, ret_angle;
    bool m_isDithering;

    QFile logFile;
    QPixmap profilePixmap;


    // IMPORTED FROM R_CALIBRATION - CLEAN UP
    void fillInterface( void );
    void calibrateManualReticle( void );
    void calibrateRADECRecticle( bool ra_only ); // 1 or 2-axis calibration

    bool startCalibration();
    bool stopCalibration();
    void processCalibration();


    void reset();

    bool is_started;

    int  axis;
    int  auto_drift_time;
    int turn_back_time;
    double start_x1, start_y1;
    double end_x1, end_y1;
    double start_x2, start_y2;
    double end_x2, end_y2;
    int iterations, dec_iterations;
    double phi;
    Matrix ROT_Z;    

    QColor idleColor, okColor, busyColor, alertColor;

    CalibrationStage calibrationStage;
    CalibrationType  calibrationType;

};

}

#endif // INTERNALGUIDER_H
