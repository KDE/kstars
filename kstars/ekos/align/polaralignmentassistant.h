/*  Ekos Polar Alignment Assistant Tool
    Copyright (C) 2018-2021 by Jasem Mutlaq
    Copyright (C) 2020-2021 by Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_polaralignmentassistant.h"
#include "ekos/ekos.h"
#include "ekos/guide/internalguide/starcorrespondence.h"
#include "polaralign.h"
#include "alignview.h"
#include "align.h"
#include "indi/inditelescope.h"

class QProgressIndicator;

namespace Ekos
{

class PolarAlignmentAssistant : public QWidget, public Ui::PolarAlignmentAssistant
{
        Q_OBJECT

    public:
        explicit PolarAlignmentAssistant(Align *parent, AlignView *view);
        ~PolarAlignmentAssistant();

        typedef enum
        {
            PAH_IDLE,
            PAH_FIRST_CAPTURE,
            PAH_FIND_CP,
            PAH_FIRST_ROTATE,
            PAH_SECOND_CAPTURE,
            PAH_SECOND_ROTATE,
            PAH_THIRD_CAPTURE,
            PAH_STAR_SELECT,
            PAH_PRE_REFRESH,
            PAH_REFRESH,
            PAH_ERROR
        } PAHStage;

        enum CircleSolution
        {
            NO_CIRCLE_SOLUTION,
            ONE_CIRCLE_SOLUTION,
            TWO_CIRCLE_SOLUTION,
            INFINITE_CIRCLE_SOLUTION
        };
        typedef enum { NORTH_HEMISPHERE, SOUTH_HEMISPHERE } HemisphereType;


        void setCurrentTelescope(ISD::Telescope *scope) { m_CurrentTelescope = scope; }
        void setMountSpeed();

        void setEnabled(bool enabled);

        void syncStage();

        PAHStage getPAHStage() const
        {
            return m_PAHStage;
        }

        void setPAHStage(PAHStage stage);

        double getPAHExposureDuration() const
        {
            return PAHExposure->value();
        }

        void processPAHRefresh();

        bool processSolverFailure();

        void processMountRotation(const dms &ra, double settleDuration);

        /**
             * @brief processPAHStage After solver is complete, handle PAH Stage processing
             */
        void processPAHStage(double orientation, double ra, double dec, double pixscale, bool eastToTheRight);

        void startPAHProcess();
        void stopPAHProcess();

        void setWCSToggled(bool result);

        void setMountStatus(ISD::Telescope::Status newState);

        void setPAHCorrectionOffsetPercentage(double dx, double dy);
        void setPAHRefreshDuration(double value)
        {
            PAHExposure->setValue(value);
        }
        void startPAHRefreshProcess();
        void setPAHRefreshComplete();
        void setPAHSlewDone();
        void setPAHCorrectionSelectionComplete();

        // PAH Settings. PAH should be in separate class
        QJsonObject getPAHSettings() const;
        void setPAHSettings(const QJsonObject &settings);

        // PAH Ekos Live
        QString getPAHStageString() const
        {
            return PAHStages[m_PAHStage];
        }

        QString getPAHMessage() const;

        void setImageData(const QSharedPointer<FITSData> &image) { m_ImageData = image; }

    protected:        





        // Polar Alignment Helper slots
        void rotatePAH();
        void setPAHCorrectionOffset(int x, int y);


    private:

        /**
            * @brief Warns the user if the polar alignment might cross the meridian.
            */
        bool checkPAHForMeridianCrossing();


        bool detectStarsPAHRefresh(QList<Edge> *stars, int num, int x, int y, int *xyIndex);
        int refreshIteration { 0 };
        StarCorrespondence starCorrespondencePAH;

        /**
             * @brief calculatePAHError Calculate polar alignment error in the Polar Alignment Helper (PAH) method
             * @return True if calculation is successsful, false otherwise.
             */
        bool calculatePAHError();

        /**
         * @brief syncCorrectionVector Flip correction vector based on user settings.
         */
        void syncCorrectionVector();

        void setupCorrectionGraphics(const QPointF &pixel);

private:

        // Polar Alignment Helper
        PAHStage m_PAHStage { PAH_IDLE };
        SkyPoint targetPAH;
        bool isPAHReady { false };

        // Which hemisphere are we located on?
        HemisphereType hemisphere;

        // Polar alignment will retry capture & solve a few times if solve fails.
        int m_PAHRetrySolveCounter { 0 };

        // Points on the image to correct mount's ra axis.
        // correctionFrom is the star the user selected (or center of the image at start).
        // correctionTo is where theuser should move that star.
        // correctionAltTo is where the use should move that star to only fix altitude.
        QPointF correctionFrom, correctionTo, correctionAltTo;

        // PAH Stage Map
        static const QMap<PAHStage, QString> PAHStages;

        // Threshold to stop PAH rotation in degrees
        static constexpr uint8_t PAH_ROTATION_THRESHOLD { 5 };

        // Class used to estimate alignment error.
        PolarAlign polarAlign;

    signals:
        void newLog(const QString &);
        void captureAndSolve();
        void polarResultUpdated(QLineF correctionVector, double polarError, double azError, double altError);
        void newCorrectionVector(QLineF correctionVector);
        void settleStarted(double duration);

        // Polar Assistant Tool
        void newPAHStage(PAHStage stage);
        void newPAHMessage(const QString &message);
        void PAHEnabled(bool);

        void newAlignTableResult(Align::AlignResult result);

        // This is sent when the pixmap is updated within the view
        void newFrame(FITSView *view);



    private:

        QSharedPointer<FITSData> m_ImageData;
        Align *m_AlignInstance {nullptr};
        ISD::Telescope *m_CurrentTelescope { nullptr };
        AlignView *alignView { nullptr };
};
}
