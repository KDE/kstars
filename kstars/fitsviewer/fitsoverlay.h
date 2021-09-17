/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPixmap>
#include <KIO/Job>
#include <QFile>

#include "dms.h"

class FITSImage;
class SkyPoint;

typedef struct
{
    FITSImage *image_data;
    dms ra, dec;
    int pix_width, pix_height;
    QPixmap pix;
} FOverlay;

class FITSOverlay : public QObject
{
    Q_OBJECT

  public:
    FITSOverlay();
    ~FITSOverlay();

    void addFITSOverlay(const dms &ra, const dms &dec, const QUrl &imageURL);
    //void removeFITSOverlay(const SkyPoint *clickedPoint);

    //const QPixmap *getPixmap() { return &pix;}
    //const SkyPoint *getCoord() { return coord; }

    bool contains(const dms &ra, const dms &dec);

    QList<FOverlay *> getOverlays() { return fList; }

  private:
    void loadImageFromURL();
    void openImage();

    QUrl m_ImageUrl;
    KIO::Job *downloadJob { nullptr }; // download job of image -> 0 == no job is running
    QString filename;
    QFile file;
    dms ra, dec;

    QList<FOverlay *> fList;

  private slots:
    /**Make sure download has finished, then make sure file exists, then save the image */
    void downloadReady(KJob *);
};
