/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

class QQuickView;
class QQuickItem;
class QQmlContext;

namespace Ekos
{

class MosaicPlanner : public QWidget
{
        Q_OBJECT
        Q_PROPERTY(double focalLength MEMBER m_FocalLength NOTIFY focalLengthChanged)
        Q_PROPERTY(QSize cameraSize MEMBER m_CameraSize NOTIFY cameraSizeChanged)
        Q_PROPERTY(QSizeF pixelSize MEMBER m_PixelSize NOTIFY pixelSizeChanged)
        Q_PROPERTY(QSize gridSize MEMBER m_GridSize NOTIFY gridSizeChanged)
        Q_PROPERTY(double overlap MEMBER m_Overlap NOTIFY overlapChanged)

    public:
        MosaicPlanner(QWidget *parent = nullptr);
        ~MosaicPlanner() override;

        // Return Camera Field of View in arcminutes
        Q_INVOKABLE QSizeF cameraFOV() const
        {
            return m_cameraFOV;
        }
        // Return Mosaic Field of View in arcminutes
        Q_INVOKABLE QSizeF mosaicFOV() const
        {
            return m_MosaicFOV;
        }

    public:        

    protected:

    signals:
        void focalLengthChanged();
        void cameraSizeChanged();
        void pixelSizeChanged();
        void gridSizeChanged();
        void overlapChanged();

    private:

        QQuickView *m_BaseView = nullptr;
        QQuickItem *m_BaseObj  = nullptr;
        QQmlContext *m_Ctxt    = nullptr;

        double m_FocalLength {0};
        QSize m_CameraSize;
        QSizeF m_PixelSize, m_cameraFOV {60.1, 44.5}, m_MosaicFOV;
        QSize m_GridSize {1,1};
        double m_Overlap {10};
};
}
