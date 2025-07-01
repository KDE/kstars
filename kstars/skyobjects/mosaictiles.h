/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyobject.h"
#include "config-kstars.h"

#include <QBrush>
#include <QPen>
#include <memory>
#include <functional>

#ifdef HAVE_INDI
#include <lilxml.h>
#endif

class QPainter;

class MosaicTiles : public SkyObject
{
    public:
        MosaicTiles();
        ~MosaicTiles();

    public:

        /***************************************************************************************************
         * Import/Export Functions.
         ***************************************************************************************************/
#ifdef HAVE_INDI
        /**
         * @brief fromXML Load Scheduler XML file and parse it to create tiles in operation mode.
         * @param filename full path to filename
         * @return True if parsing is successful, false otherwise.
         */
        bool fromXML(const QString &filename);
#endif

        /**
         * @brief toJSON
         * @param output
         * @return
         */
        bool toJSON(QJsonObject &output);

        std::function<void(const QJsonObject &)> updateCallback;

        void setUpdateCallback(std::function<void(const QJsonObject &)> callback)
        {
            updateCallback = callback;
        }

        /***************************************************************************************************
         * Operation Modes
         ***************************************************************************************************/
        typedef enum
        {
            MODE_PLANNING,
            MODE_OPERATION
        } OperationMode;

        OperationMode m_OperationMode {MODE_PLANNING};
        OperationMode operationMode() const
        {
            return m_OperationMode;
        }
        void setOperationMode(OperationMode value)
        {
            m_OperationMode = value;
        }

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

        bool isValid() const;
        void createTiles(bool s_shaped);
        void appendTile(const OneTile &value);
        void appendEmptyTile();
        void clearTiles();
        void draw(QPainter *painter);

        /**
         * @brief syncFOVs Update camera and mosaic overall FOV from current settings.
         */
        void syncFOVs();

        // Getters
        // Return Sky Point
        double focalLength() const
        {
            return m_FocalLength;
        }
        double focalReducer() const
        {
            return m_FocalReducer;
        }
        double positionAngle() const
        {
            return m_PositionAngle;
        }
        QSize cameraSize() const
        {
            return m_CameraSize;
        }
        QSizeF pixelSize() const
        {
            return m_PixelSize;
        }
        QSize gridSize() const
        {
            return m_GridSize;
        }
        double overlap() const
        {
            return m_Overlap;
        }
        // Return Camera Field of View in arcminutes
        QSizeF cameraFOV() const
        {
            return m_CameraFOV;
        }
        // Return Mosaic Field of View in arcminutes
        QSizeF mosaicFOV() const
        {
            return m_MosaicFOV;
        }

        // Setters
        void setFocalLength(double value)
        {
            m_FocalLength = value;
        }
        void setFocalReducer(double value)
        {
            m_FocalReducer = value;
        }
        void setPositionAngle(double value);
        void setCameraSize(const QSize &value)
        {
            m_CameraSize = value;
        }
        void setPixelSize(const QSizeF &value)
        {
            m_PixelSize = value;
        }
        void setGridSize(const QSize &value)
        {
            m_GridSize = value;
        }
        void setOverlap(double value);
        void setCameraFOV(const QSizeF &value)
        {
            m_CameraFOV = value;
        }
        void setMosaicFOV(const QSizeF &value)
        {
            m_MosaicFOV = value;
        }
        void setPainterAlpha(int value)
        {
            m_PainterAlpha = value;
        }
        void setPainterAlphaAuto(bool value)
        {
            m_PainterAlphaAuto = value;
        }
        const QString &targetName() const
        {
            return m_TargetName;
        }
        void setTargetName(const QString &value)
        {
            m_TargetName = value;
        }
        const QString &group() const
        {
            return m_Group;
        }
        void setGroup(const QString &value)
        {
            m_Group = value;
        }
        const QString &completionCondition(QString *arg) const
        {
            *arg = m_CompletionConditionArg;
            return m_CompletionCondition;
        }
        void setCompletionCondition(const QString &value, const QString &arg = "")
        {
            m_CompletionCondition = value;
            m_CompletionConditionArg = arg;
        }

        const QString &sequenceFile() const
        {
            return m_SequenceFile;
        }
        void setSequenceFile(const QString &value)
        {
            m_SequenceFile = value;
        }
        const QString &outputDirectory() const
        {
            return m_OutputDirectory;
        }
        void setOutputDirectory(const QString &value)
        {
            m_OutputDirectory = value;
        }
        int focusEveryN() const
        {
            return m_FocusEveryN;
        }
        void setFocusEveryN(int value)
        {
            m_FocusEveryN = value;
        }
        int alignEveryN() const
        {
            return m_AlignEveryN;
        }
        void setAlignEveryN(int value)
        {
            m_AlignEveryN = value;
        }
        bool isTrackChecked() const
        {
            return m_TrackChecked;
        }
        bool isFocusChecked() const
        {
            return m_FocusChecked;
        }
        bool isAlignChecked() const
        {
            return m_AlignChecked;
        }
        bool isGuideChecked() const
        {
            return m_GuideChecked;
        }
        void setStepChecks(bool track, bool focus, bool align, bool guide)
        {
            m_TrackChecked = track;
            m_FocusChecked = focus;
            m_AlignChecked = align;
            m_GuideChecked = guide;
        }

        // Titles
        const QList<std::shared_ptr<OneTile >> &tiles() const
        {
            return m_Tiles;
        }
        std::shared_ptr<OneTile> oneTile(int row, int col);

    private:

        // Overall properties
        double m_FocalLength {0};
        double m_FocalReducer {1};
        QSize m_CameraSize;
        QSizeF m_PixelSize, m_CameraFOV, m_MosaicFOV;
        QSize m_GridSize {1, 1};
        double m_Overlap {10};
        double m_PositionAngle {0};
        bool m_SShaped {false};
        int m_PainterAlpha {50};
        bool m_PainterAlphaAuto {true};
        QString m_TargetName;
        QString m_Group;
        QString m_CompletionCondition;
        QString m_CompletionConditionArg;
        QString m_SequenceFile;
        QString m_OutputDirectory;
        int m_FocusEveryN {1};
        int m_AlignEveryN {1};
        bool m_TrackChecked {true}, m_FocusChecked {true}, m_AlignChecked {true}, m_GuideChecked {true};

        QBrush m_Brush;
        QPen m_Pen;
        QBrush m_TextBrush;
        QPen m_TextPen;

        QList<std::shared_ptr<OneTile >> m_Tiles;

        /**
           * @brief adjustCoordinate This uses the mosaic center as reference and the argument resolution of the sky map at that center.
           * @param tileCoord point to adjust
           * @return Returns scaled offsets for a pixel local coordinate.
           */
        QSizeF adjustCoordinate(QPointF tileCoord);
        void updateTiles();

        bool processJobInfo(XMLEle *root, int index);

        QPointF rotatePoint(QPointF pointToRotate, QPointF centerPoint, double paDegrees);

        QSizeF calculateTargetMosaicFOV() const;
        QSize mosaicFOVToGrid() const;
        QSizeF calculateCameraFOV() const;
};
