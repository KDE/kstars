/*  Ekos Alignment View
 *  Child of FITSView with few additions necessary for Alignment functions

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/


#ifndef ALIGNVIEW_H
#define ALIGNVIEW_H

#include "fitsviewer/fitsview.h"

class AlignView : public FITSView
{
    Q_OBJECT
public:
    AlignView(QWidget *parent = 0, FITSMode mode=FITS_NORMAL, FITSScale filter=FITS_NONE);
    ~AlignView();

    /* Calculate WCS header info and update WCS info */
    bool createWCSFile(const QString & newWCSFile, double orientation, double ra, double dec, double pixscale);

    void drawOverlay(QPainter *) override;

    // Correction line
    void setCorrectionParams(QLineF line);
    void setCorrectionOffset(QPointF newOffset);

    void setRACircle(const QVector3D &value);

protected:
    void drawLine(QPainter *painter);
    void drawCircle(QPainter *painter);

private:
    // Correction Line
    QLineF correctionLine;
    QPointF correctionCenter, correctionOffset;
    QVector3D RACircle;
};

#endif // ALIGNVIEW_H
