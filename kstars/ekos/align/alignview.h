/*  Ekos Alignment View
 *  Child of FITSView with few additions necessary for Alignment functions

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "fitsviewer/fitsview.h"

#include <QVector3D>

class QPainter;

class AlignView : public FITSView
{
    Q_OBJECT
  public:
    explicit AlignView(QWidget *parent = nullptr, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE);
    ~AlignView() = default;

    /* Calculate WCS header info and update WCS info */
    bool createWCSFile(const QString &newWCSFile, double orientation, double ra, double dec, double pixscale);

    void drawOverlay(QPainter *) override;

    // Correction line
    void setCorrectionParams(QLineF &line);
    void setCorrectionOffset(QPointF &newOffset);

    void setRACircle(const QVector3D &value);
    void setRefreshEnabled(bool enable);

  protected:
    void drawLine(QPainter *painter);
    void drawCircle(QPainter *painter);

  private:
    // Correction Line
    QLineF correctionLine;
    QPointF correctionOffset, celestialPolePoint;
    QVector3D RACircle;

signals:
    void newCorrectionVector(QLineF correctionVector);
};
