/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"
#include <lilxml.h>
#include <memory>

namespace Ekos
{

/**
 * @brief The MosaicTilesModel class holds the data representation of the mosaic tiles. These represent metadata describing the
 * overall mosaic, and tile-per-tile specific metadata.
 */
class MosaicTilesModel : public QObject
{
        Q_OBJECT
        Q_PROPERTY(double focalLength MEMBER m_FocalLength READ focalLength WRITE setFocalLength NOTIFY focalLengthChanged)
        Q_PROPERTY(double focalReducer MEMBER m_FocalReducer READ focalReducer WRITE setFocalReducer NOTIFY focalReducerChanged)
        Q_PROPERTY(double positionAngle MEMBER m_PositionAngle READ positionAngle WRITE setPositionAngle NOTIFY positionAngleChanged)
        Q_PROPERTY(QSize cameraSize MEMBER m_CameraSize READ cameraSize WRITE setCameraSize NOTIFY cameraSizeChanged)
        Q_PROPERTY(QSizeF pixelSize MEMBER m_PixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged)
        Q_PROPERTY(QSizeF pixelScale MEMBER m_PixelScale READ pixelScale WRITE setPixelScale NOTIFY pixelScaleChanged)
        Q_PROPERTY(QSize gridSize MEMBER m_GridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
        Q_PROPERTY(double overlap MEMBER m_Overlap READ overlap WRITE setOverlap NOTIFY overlapChanged)
        Q_PROPERTY(SkyPoint skycenter MEMBER m_SkyCenter READ skyCenter WRITE setSkyCenter NOTIFY skycenterChanged)

    public:
        MosaicTilesModel(QObject *parent = nullptr);
        ~MosaicTilesModel() override;

        /***************************************************************************************************
         * Import/Export Functions.
         ***************************************************************************************************/
        /**
         * @brief toXML
         * @param output
         * @return
         */
        bool toXML(const QTextStream &output);

        /**
         * @brief fromXML
         * @param root
         * @return
         */
        bool fromXML(XMLEle *root);

        /**
         * @brief toJSON
         * @param output
         * @return
         */
        bool toJSON(QJsonObject &output);

        /**
         * @brief fromJSON
         * @param input
         * @return
         */
        bool fromJSON(const QJsonObject &input);

        /***************************************************************************************************
         * Tile Functions.
         ***************************************************************************************************/
        typedef struct
        {
            QPointF pos;
            QPointF center;
            SkyPoint skyCenter;
            double rotation;
            int index;
        } OneTile;

        void appendTile(const OneTile &value);
        void appendEmptyTile();
        void clearTiles();

        // Getters

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

        // Return Sky Point
        double focalLength() const {return m_FocalLength;}
        double focalReducer() const {return m_FocalReducer;}
        double positionAngle() const {return m_PositionAngle;}
        QSize cameraSize() const {return m_CameraSize;}
        QSizeF pixelSize() const {return m_PixelSize;}
        QSizeF pixelScale() const {return m_PixelScale;}
        QSize gridSize() const {return m_GridSize;}
        double overlap() const {return m_Overlap;}
        const SkyPoint &skyCenter() const {return m_SkyCenter;}

        // Setters
        void setFocalLength(double value) {m_FocalLength = value;}
        void setFocalReducer(double value) {m_FocalReducer = value;}
        void setPositionAngle(double value);
        void setCameraSize(const QSize &value) { m_CameraSize = value;}
        void setPixelSize(const QSizeF &value) { m_PixelSize = value;}
        void setPixelScale(const QSizeF &value) { m_PixelScale = value;}
        void setGridSize(const QSize &value) {m_GridSize = value;}
        void setSkyCenter(const SkyPoint &value) { m_SkyCenter = value;}
        void setOverlap(double value);

        // Titles
        const QList<std::shared_ptr<OneTile>> & tiles() const {return m_Tiles;}
        std::shared_ptr<MosaicTilesModel::OneTile> oneTile(int row, int col);

    protected:

    signals:
        void focalLengthChanged();
        void focalReducerChanged();
        void cameraSizeChanged();
        void pixelSizeChanged();
        void pixelScaleChanged();
        void gridSizeChanged();
        void overlapChanged();
        void positionAngleChanged();
        void skycenterChanged();

    private:

        // Overall properties
        double m_FocalLength {0};
        double m_FocalReducer {1};
        QSize m_CameraSize;
        QSizeF m_PixelSize, m_PixelScale, m_cameraFOV, m_MosaicFOV;
        QSize m_GridSize {1,1};
        double m_Overlap {10};
        double m_PositionAngle {0};
        SkyPoint m_SkyCenter;

        QList<std::shared_ptr<OneTile>> m_Tiles;
};
}
