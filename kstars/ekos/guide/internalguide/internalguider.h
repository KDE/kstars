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

#include "../guideinterface.h"

namespace Ekos
{

class InternalGuider : public GuideInterface
{

public:

    enum CalibrationStage { CAL_CAPTURE_IMAGE, CAL_SELECT_STAR, CAL_FINISH, CAL_ERROR, CAL_START, CAL_RA_INC, CAL_RA_DEC, CAL_DEC_INC, CAL_DEC_DEC };
    enum CalibrationType { CAL_NONE, CAL_MANUAL, CAL_RA_AUTO, CAL_RA_DEC_AUTO };

    InternalGuider();
    ~InternalGuider();

    void Connect() override;
    void Disconnect() override;

    bool calibrate() override;
    bool guide() override;
    bool stop() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;

    void setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture, double mountFocalLength) override;

    void setSquareAlgorithm( int index );



    /// IMPORTED CHECK THEM ALL

    void guide( void );
    bool start();
    bool stop();
    bool abort(bool silence=false);
    void setHalfRefreshRate( bool is_half );
    bool isGuiding( void ) const;
    void setMathObject( cgmath *math );
    void setAO(bool enable);
    void setInterface( void );
    void setImageView(FITSView *image);
    void setReady(bool enable) { m_isReady = enable;}
    void setTargetChip(ISD::CCDChip *chip);
    bool isRapidGuide() { return m_useRapidGuide;}

    double getAOLimit();
    void setSubFramed(bool enable) { m_isSubFramed = enable;}
    void setGuideOptions(const QString & algorithm, bool useSubFrame, bool useRapidGuide);

    // Dither
    bool isDitherChecked() { return ui.ditherCheck->isChecked(); }
    bool dither();
    bool isDithering() { return m_isDithering; }
    void setDither(bool enable, double value);
    double getDitherPixels() { return ui.ditherPixels->value(); }

    setSquareAlgorithm( index );

    QString getAlgorithm();
    bool useSubFrame();
    bool useRapidGuide();

    void setPHD2(Ekos::PHD2 *phd);

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
    ISD::CCDChip *targetChip;
    int fx,fy,fw,fh;
    double ret_x, ret_y, ret_angle;
    bool m_isDithering;
    QFile logFile;
    QPixmap profilePixmap;


    // IMPORTED FROM R_CALIBRATION - CLEAN UP
    void fillInterface( void );
    void calibrateManualReticle( void );
    void calibrateRADECRecticle( bool ra_only ); // 1 or 2-axis calibration

    bool is_started;

    calibrationparams_t calibration_params;
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

    cgmath *pmath;
    Ekos::Guide *guideModule;

    QColor idleColor, okColor, busyColor, alertColor;

    CalibrationStage calibrationStage;
    CalibrationType  calibrationType;

    QPointer<FITSView> guideFrame;
};

}

#endif // INTERNALGUIDER_H
