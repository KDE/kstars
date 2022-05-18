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
class FramingAssistant;
}

namespace Ekos
{
class Scheduler;
class MosaicTilesManager;
class MosaicTilesScene;

class FramingAssistant : public QDialog
{
        Q_OBJECT
        Q_PROPERTY(double focalLength MEMBER m_FocalLength)
        Q_PROPERTY(double overlap MEMBER m_Overlap)
        Q_PROPERTY(double rotation MEMBER m_Rotation)
        Q_PROPERTY(QSize cameraSize MEMBER m_CameraSize)
        Q_PROPERTY(QSizeF pixelSize MEMBER m_PixelSize)
        Q_PROPERTY(QSize gridSize MEMBER m_GridSize)

    public:
        static FramingAssistant *Instance();

    public:
        Ui::FramingAssistant* ui { nullptr };

        void setCenter(const SkyPoint &value);
        QString getJobsDir() const;
        void syncModelToGUI();
        void syncGUIToModel();

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

        /**
         * @brief goAndSolve Go to current center, capture an image, and solve.
         */
        void goAndSolve();

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

    private:

        FramingAssistant();
        ~FramingAssistant() override;

        static FramingAssistant *_FramingAssistant;

        SkyPoint m_CenterPoint;
        QImage *m_skyChart { nullptr };

        QPixmap targetPix;
        QGraphicsPixmapItem *m_SkyPixmapItem { nullptr };

        MosaicTilesManager *m_MosaicTilesManager { nullptr };

        double pixelsPerArcminRA { 0 }, pixelsPerArcminDE { 0 };
        double renderedWFOV { 0 }, renderedHFOV { 0 };
        double premosaicZoomFactor { 0 };

        QPointF screenPoint;
        QGraphicsScene m_TilesScene;

        bool m_RememberAltAzOption {false}, m_RememberShowGround {false};

        QTimer *updateTimer { nullptr };

        // Equipment
        double m_FocalLength {0};
        QSize m_CameraSize;
        QSizeF m_PixelSize, m_cameraFOV, m_MosaicFOV;
        QSize m_GridSize {1,1};
        double m_Overlap {10}, m_Rotation {0};
};
}
