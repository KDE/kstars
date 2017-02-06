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
    bool updateWCS(double orientation, double ra, double dec, double pixscale);

    void drawOverlay(QPainter *) override;

    // Correction line
    void setCorrectionParams(QLine line, QPoint center);
    void setCorrectionOffset(QPoint newOffset);

protected:
    void drawLine(QPainter *painter);

private:
    // Correction Line
    QLine correctionLine;
    QPoint correctionCenter, correctionOffset;
};

#endif // ALIGNVIEW_H
