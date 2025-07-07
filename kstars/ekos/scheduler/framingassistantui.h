/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"
#include "ekos/ekos.h"
#include "indi/indimount.h"

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

class FramingAssistantUI : public QDialog
{
        Q_OBJECT

        Q_PROPERTY(QSize gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
        Q_PROPERTY(double overlap READ overlap WRITE setOverlap NOTIFY overlapChanged)
        Q_PROPERTY(double positionAngle READ positionAngle WRITE setPositionAngle NOTIFY positionAngleChanged)
        Q_PROPERTY(QString sequenceFile READ sequenceFile WRITE setSequenceFile NOTIFY sequenceFileChanged)
        Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
        Q_PROPERTY(QString targetName READ targetName WRITE setTargetName NOTIFY targetNameChanged)
        Q_PROPERTY(int focusEveryN READ focusEveryN WRITE setFocusEveryN NOTIFY focusEveryNChanged)
        Q_PROPERTY(int alignEveryN READ alignEveryN WRITE setAlignEveryN NOTIFY alignEveryNChanged)
        Q_PROPERTY(bool isTrackChecked READ isTrackChecked WRITE setIsTrackChecked NOTIFY isTrackCheckedChanged)
        Q_PROPERTY(bool isFocusChecked READ isFocusChecked WRITE setIsFocusChecked NOTIFY isFocusCheckedChanged)
        Q_PROPERTY(bool isAlignChecked READ isAlignChecked WRITE setIsAlignChecked NOTIFY isAlignCheckedChanged)
        Q_PROPERTY(bool isGuideChecked READ isGuideChecked WRITE setIsGuideChecked NOTIFY isGuideCheckedChanged)
        Q_PROPERTY(QString completionCondition READ completionCondition WRITE setCompletionCondition NOTIFY
                   completionConditionChanged)
        Q_PROPERTY(QString completionConditionArg READ completionConditionArg WRITE setCompletionConditionArg NOTIFY
                   completionConditionArgChanged)
    public:

        FramingAssistantUI();
        ~FramingAssistantUI() override;

    public:
        enum
        {
            PAGE_EQUIPMENT,
            PAGE_SELECT_GRID,
            PAGE_ADJUST_GRID,
            PAGE_CREATE_JOBS
        };

        // Import Mosaic JSON Data
        bool importMosaic(const QJsonObject &payload);

    public:
        Q_INVOKABLE void setGridSize(const QSize &value);
        const QSize &gridSize() const
        {
            return m_GridSize;
        }

        Q_INVOKABLE void setOverlap(double value);
        double overlap() const
        {
            return m_Overlap;
        }

        Q_INVOKABLE void setPositionAngle(double);
        double positionAngle() const
        {
            return m_PA;
        }

        Q_INVOKABLE void setSequenceFile(const QString &value);
        const QString &sequenceFile() const
        {
            return m_SequenceFile;
        }

        Q_INVOKABLE void setOutputDirectory(const QString &value);
        const QString &outputDirectory() const
        {
            return m_OutputDirectory;
        }

        Q_INVOKABLE void setTargetName(const QString &value);
        const QString &targetName() const
        {
            return m_TargetName;
        }

        Q_INVOKABLE void setFocusEveryN(int value);
        int focusEveryN() const
        {
            return m_FocusEveryN;
        }

        Q_INVOKABLE void setAlignEveryN(int value);
        int alignEveryN() const
        {
            return m_AlignEveryN;
        }

        Q_INVOKABLE void setIsTrackChecked(bool value);
        bool isTrackChecked() const
        {
            return m_IsTrackChecked;
        }

        Q_INVOKABLE void setIsFocusChecked(bool value);
        bool isFocusChecked() const
        {
            return m_IsFocusChecked;
        }

        Q_INVOKABLE void setIsAlignChecked(bool value);
        bool isAlignChecked() const
        {
            return m_IsAlignChecked;
        }

        Q_INVOKABLE void setIsGuideChecked(bool value);
        bool isGuideChecked() const
        {
            return m_IsGuideChecked;
        }

        Q_INVOKABLE void setCompletionCondition(const QString &value);
        const QString &completionCondition() const
        {
            return m_CompletionCondition;
        }

        Q_INVOKABLE void setCompletionConditionArg(const QString &value);
        const QString  &completionConditionArg() const
        {
            return m_CompletionConditionArg;
        }

        Q_INVOKABLE void createJobs();

        /**
         * @brief setCenter Sets the center point of the framing assistant.
         * @param ra0 Right Ascension in degrees.
         * @param dec0 Declination in degrees.
         */
        Q_INVOKABLE void setCenter(double ra0, double dec0);

    protected:
        /// @brief Camera information validity checker.
        bool isEquipmentValid() const;

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

        /**
         * @brief goAndRotate Set position angle then go and solve.
         */
        void goAndRotate();

        // Select sequence file
        void selectSequence();
        // Select jobs directory
        void selectDirectory();
        // Select mosaic import
        void selectImport();
        // Import mosaic CSV
        bool parseMosaicCSV(const QString &filename);
        // Sanitize target name
        void sanitizeTarget();

    public slots:
        void updateTargetFOVFromGrid();
        void updateGridFromTargetFOV();
        void constructMosaic();
        void calculateFOV();
        void resetFOV();
        void fetchINDIInformation();
        void rewordStepEvery(int v);
        void setMountState(ISD::Mount::Status value);
        void setAlignState(AlignState value);

    signals:
        void gridSizeChanged(const QSize &value);
        void overlapChanged(double value);
        void positionAngleChanged(double value);
        void sequenceFileChanged(const QString &value);
        void outputDirectoryChanged(const QString &value);
        void targetNameChanged(const QString &value);
        void focusEveryNChanged(int value);
        void alignEveryNChanged(int value);
        void isTrackCheckedChanged(bool value);
        void isFocusCheckedChanged(bool value);
        void isAlignCheckedChanged(bool value);
        void isGuideCheckedChanged(bool value);
        void completionConditionChanged(const QString &value);
        void completionConditionArgChanged(const QString &value);

    private:

        SkyPoint m_CenterPoint;
        Ui::FramingAssistant *ui {nullptr};

        double renderedWFOV { 0 }, renderedHFOV { 0 };
        QTimer *m_DebounceTimer { nullptr };

        // Go and solve
        bool m_GOTOSolvePending {false};
        AlignState m_AlignState {ALIGN_IDLE};
        ISD::Mount::Status m_MountState {ISD::Mount::MOUNT_IDLE};

        // Equipment
        double m_FocalLength {0};
        double m_FocalReducer {1};
        QSize m_CameraSize, m_GridSize;
        QSizeF m_PixelSize, m_cameraFOV, m_MosaicFOV;
        double m_PA {0}, m_Overlap {10};
        QString m_JobsDirectory;
        QString m_SequenceFile;
        QString m_OutputDirectory;
        QString m_TargetName;
        int m_FocusEveryN { 0 };
        int m_AlignEveryN { 0 };
        bool m_IsTrackChecked { false };
        bool m_IsFocusChecked { false };
        bool m_IsAlignChecked { false };
        bool m_IsGuideChecked { false };
        QString m_CompletionCondition {"FinishSequence"};
        QString m_CompletionConditionArg { "1" };
};
}
