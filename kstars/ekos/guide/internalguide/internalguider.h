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

#include "indi/indicommon.h"
#include "fitsviewer/fitsview.h"

#include "matr.h"
#include "../guideinterface.h"

class cgmath;

namespace Ekos
{

class InternalGuider : public GuideInterface
{
    Q_OBJECT

public:

    enum CalibrationStage { CAL_IDLE, CAL_ERROR, CAL_CAPTURE_IMAGE, CAL_SELECT_STAR, CAL_START, CAL_RA_INC, CAL_RA_DEC, CAL_DEC_INC, CAL_DEC_DEC };
    enum CalibrationType { CAL_NONE, CAL_RA_AUTO, CAL_RA_DEC_AUTO };

    InternalGuider();
    ~InternalGuider();

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
    bool setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture, double mountFocalLength) override;

    // Set Star Position
    void setStarPosition(QVector3D starCenter) override;

    // Select algorithm
    void setSquareAlgorithm( int index );

    // Reticle Parameters
    void setReticleParameters(double x, double y, double angle);
    bool getReticleParameters(double *x, double *y, double *angle);

    // Guide View
    void setGuideView(FITSView *guideView);



    /// IMPORTED CHECK THEM ALL

    bool start();
    //bool abort(bool silence=false);

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
    //void connectPHD2();
    //void setPHD2Connected();
    //void setPHD2Disconnected();
    // Only called by PHD2
    //void toggleExternalGuideStateGUI(Ekos::GuideState state);

protected slots:
    //void openCalibrationOptions();
    //void openGuideOptions();

    //void capture();
    void trackingStarSelected(int x, int y);


signals:    
    void newPulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    void newPulse( GuideDirection dir, int msecs );
    void DESwapChanged(bool enable);

private:

    // Calibration
    void calibrateRADECRecticle( bool ra_only ); // 1 or 2-axis calibration
    void processCalibration();

    // Guiding
    bool processGuiding();

    cgmath *pmath;
    QPointer<FITSView> guideFrame;
    bool m_isStarted;
    bool m_isReady;
    bool m_isSubFramed;
    bool isFirstFrame, first_subframe;
    bool half_refresh_rate;
    int m_lostStarTries;
    bool m_useRapidGuide;    
    int fx,fy,fw,fh;
    double ret_x, ret_y, ret_angle;
    bool m_isDithering;

    QFile logFile;    

    void reset();

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

    CalibrationStage calibrationStage;
    CalibrationType  calibrationType;

};

}

#endif // INTERNALGUIDER_H
