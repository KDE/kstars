/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"

#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QDialog>
#include <QBrush>
#include <QPen>

namespace Ui
{
class mosaicDialog;
}

namespace Ekos
{
class Scheduler;
class MosaicTile;

class Mosaic : public QDialog
{
        Q_OBJECT

    public:
        Mosaic(QString targetName, SkyPoint center, QWidget *parent = nullptr);
        ~Mosaic() override;

    public:
        Ui::mosaicDialog* ui { nullptr };

        void setCameraSize(uint16_t width, uint16_t height);
        void setPixelSize(double pixelWSize, double pixelHSize);
        void setFocalLength(double focalLength);
        void setCenter(const SkyPoint &value);
        QString getJobsDir() const;

    public:
        typedef struct _Job
        {
            SkyPoint center;
            double rotation;
            bool doAlign;
            bool doFocus;
        } Job;

        QList <Job> getJobs() const;

    protected:
        virtual void showEvent(QShowEvent *) override;
        virtual void resizeEvent(QResizeEvent *) override;

        /// @brief Camera information validity checker.
        bool isScopeInfoValid() const;

        /// @brief Expected arcmin field width for the current number of tiles.
        double getTargetWFOV() const;

        /// @brief Expected arcmin field height for the current number of tiles.
        double getTargetHFOV() const;

        /// @brief Expected number of tiles for the current target field width.
        double getTargetMosaicW() const;

        /// @brief Expected number of tiles for the current target field height.
        double getTargetMosaicH() const;

    public slots:
        void updateTargetFOVFromGrid();
        void updateGridFromTargetFOV();
        void constructMosaic();
        void calculateFOV();
        void updateTargetFOV();
        void saveJobsDirectory();
        void resetFOV();
        void fetchINDIInformation();
        void rewordStepEvery(int v);

    public slots:
        virtual int exec() override;
        virtual void accept() override;

    private:
        SkyPoint center;
        QImage *m_skyChart { nullptr };

        QPixmap targetPix;
        QGraphicsPixmapItem *skyMapItem { nullptr };

        MosaicTile *mosaicTileItem { nullptr };

        double pixelsPerArcminRA { 0 }, pixelsPerArcminDE { 0 };
        double renderedWFOV { 0 }, renderedHFOV { 0 };
        double premosaicZoomFactor { 0 };

        QPointF screenPoint;
        QGraphicsScene scene;

        bool rememberAltAzOption;

        QTimer *updateTimer { nullptr };
};
}
