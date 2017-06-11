/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef GUIDER_H
#define GUIDER_H

#include <QtGui>

#include "common.h"
#include "ui_guider.h"
#include "scroll_graph.h"
#include "gmath.h"

#include "../guide.h"

class internalGuider : public QWidget
{
    Q_OBJECT

  public:
    explicit internalGuider(cgmath *mathObject, Ekos::Guide *parent = 0);
    ~internalGuider();

    void guide(void);
    bool start();
    bool stop();
    bool abort(bool silence = false);
    void setHalfRefreshRate(bool is_half);
    bool isGuiding(void) const;
    void setMathObject(cgmath *math);
    void setAO(bool enable);
    void setInterface(void);
    void setImageView(FITSView *image);
    void setReady(bool enable) { m_isReady = enable; }
    void setTargetChip(ISD::CCDChip *chip);
    bool isRapidGuide() { return m_useRapidGuide; }

    double getAOLimit();
    void setSubFramed(bool enable) { m_isSubFramed = enable; }
    void setGuideOptions(const QString &algorithm, bool useSubFrame, bool useRapidGuide);

    // Dither
    bool isDitherChecked() { return ui.ditherCheck->isChecked(); }
    bool dither();
    bool isDithering() { return m_isDithering; }
    void setDither(bool enable, double value);
    double getDitherPixels() { return ui.ditherPixels->value(); }

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
    void onXscaleChanged(int i);
    void onYscaleChanged(int i);
    void onThresholdChanged(int i);
    void onInfoRateChanged(double val);
    void onEnableDirRA(int state);
    void onEnableDirDEC(int state);
    void onInputParamChanged();
    void onRapidGuideChanged(bool enable);
    void capture();
    void trackingStarSelected(int x, int y);
    void onStartStopButtonClick();
    void onSetDECSwap(bool enable);

  signals:
    //void ditherComplete();
    void ditherToggled(bool);
    //void autoGuidingToggled(bool);
    void newStatus(Ekos::GuideState);
    void newProfilePixmap(QPixmap &);
    void newStarPosition(QVector3D, bool);

  private:
    cgmath *pmath;
    Ekos::Guide *guideModule;
    Ekos::PHD2 *phd2;

    custom_drawer *pDriftOut;
    ScrollGraph *driftGraph;

    QPointer<FITSView> guideFrame;
    bool m_isStarted;
    bool m_isReady;
    bool m_isSubFramed;
    bool first_frame, first_subframe;
    bool half_refresh_rate;
    int m_lostStarTries;
    bool m_useRapidGuide;
    ISD::CCDChip *targetChip;
    int fx, fy, fw, fh;
    double ret_x, ret_y, ret_angle;
    bool m_isDithering;
    QFile logFile;
    QPixmap profilePixmap;

  private:
    Ui::guiderClass ui;
};

#endif // GUIDER_H
