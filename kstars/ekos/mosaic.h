/*  Ekos Mosaic Tool
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef MOSAIC_H
#define MOSAIC_H

#include <QDialog>

#include "skypoint.h"

#include "ui_mosaic.h"

namespace Ekos
{

class Scheduler;

class Mosaic : public QDialog, public Ui::mosaicDialog
{

    Q_OBJECT

public:
    Mosaic(Scheduler *scheduler);
    ~Mosaic();

    void setCameraSize(uint16_t width, uint16_t height);
    void setPixelSize(double pixelWSize, double pixelHSize);
    void setFocalLength(double focalLength);

    void setCenter(const SkyPoint &value);



protected:
    virtual void resizeEvent(QResizeEvent *);

public slots:
    void calculateFOV();
    void drawTargetFOV();
    void drawOverlay();
    void resetFOV();
    void setPreset();

    /**
     * @short Re-renders the view
     */
    void render();

private:

    void createOverlay();
    Scheduler *ekosScheduler;
    SkyPoint center;
    QImage *m_skyChart;
    QImage *m_skyImage;

    QList<SkyPoint *> jobCenter;

};

}

#endif  // MOSAIC_H
