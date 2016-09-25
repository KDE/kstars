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
    void connectPHD2();
    void setPHD2Connected();
    void setPHD2Disconnected();
    // Only called by PHD2
    void toggleExternalGuideStateGUI(Ekos::GuideState state);

protected slots:
    void onStartStopButtonClick();

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
};

}

#endif // INTERNALGUIDER_H
