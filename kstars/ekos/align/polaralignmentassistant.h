/*  Ekos Polar Alignment Assistant Tool
    SPDX-FileCopyrightText: 2018-2021 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020-2021 Hy Murveit

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ui_polaralignmentassistant.h"
#include "ekos/ekos.h"
#include "ekos/guide/internalguide/starcorrespondence.h"
#include "polaralign.h"
#include "align.h"
#include "indi/indimount.h"

class AlignView;
class QProgressIndicator;
class SolverUtils;

namespace Ekos
{

class PolarAlignWidget;

/**
 * @brief The PolarAlignmentAssistant class
 *
 * Captures three images rotated by a set number of degrees decided by the user (default 30).
 * Each image is plate solver to find the center RA,DE coordinates. The three points are then
 * used to generate a unique circle with its center as the RA axis. As the mount rotated around
 * these point, we can identify the RA rotational axis. From there, we compare the distance from RA axis
 * to the celestial pole axis. For a perfectly aligned mount, the two points would overlap exactly.
 * In reality, there is also some differences due to the mount mechanical limitations and measurements
 * errors.
 *
 * The user is then presented with a triangle that couples the corrections required in Altitude and Azimuth
 * knobs to move the RA axis to the celestial pole. An optional feature is available the calculates the error
 * in real time as the user move the mount around during refresh, but this feature is computationally intensive
 * as we need to extract stars from each frame.
 *
 * @author Jasem Mutlaq
 * @author Hy Murveit
 */
class PolarAlignmentAssistant : public QWidget, public Ui::PolarAlignmentAssistant
{
        Q_OBJECT

    public:
        explicit PolarAlignmentAssistant(Align *parent, const QSharedPointer<AlignView> &view);
        ~PolarAlignmentAssistant();

        typedef enum
        {
            PAH_IDLE,
            PAH_FIRST_CAPTURE,
            PAH_FIRST_SOLVE,
            PAH_FIND_CP,
            PAH_FIRST_ROTATE,
            PAH_FIRST_SETTLE,
            PAH_SECOND_CAPTURE,
            PAH_SECOND_SOLVE,
            PAH_SECOND_ROTATE,
            PAH_SECOND_SETTLE,
            PAH_THIRD_CAPTURE,
            PAH_THIRD_SOLVE,
            PAH_STAR_SELECT,
            PAH_REFRESH,
            PAH_POST_REFRESH
        } Stage;

        // Algorithm choice in UI
        typedef enum
        {
            PLATE_SOLVE_ALGORITHM,
            MOVE_STAR_ALGORITHM,
            MOVE_STAR_UPDATE_ERR_ALGORITHM
        } RefreshAlgorithm;

        enum CircleSolution
        {
            NO_CIRCLE_SOLUTION,
            ONE_CIRCLE_SOLUTION,
            TWO_CIRCLE_SOLUTION,
            INFINITE_CIRCLE_SOLUTION
        };
        typedef enum { NORTH_HEMISPHERE, SOUTH_HEMISPHERE } HemisphereType;

        // Set the mount used in Align class.
        void setCurrentTelescope(ISD::Mount *scope)
        {
            m_CurrentTelescope = scope;
        }
        // Sync mount slew speed and available rates from the telescope object
        void syncMountSpeed(const QString &speed);
        // Enable PAA if the FOV is sufficient
        void setEnabled(bool enabled);
        // Return the exposure used in the refresh phase.
        double getPAHExposureDuration() const
        {
            return pAHExposure->value();
        }
        // Handle updates during the refresh phase such as error estimation.
        void processPAHRefresh();
        // Handle solver failure and retry to capture until a preset number of retries is met.
        bool processSolverFailure();
        // Handle both automated and manual mount rotations.
        void processMountRotation(const dms &ra, double settleDuration);
        // After solver is complete, handle PAH Stage processing
        void processPAHStage(double orientation, double ra, double dec, double pixscale, bool eastToTheRight, short healpix,
                             short index);
        // Return current PAH stage
        Stage getPAHStage() const
        {
            return m_PAHStage;
        }
        // Set active stage.
        void setPAHStage(Stage stage);
        // Start the polar alignment process.
        void startPAHProcess();
        // Stops the polar alignment process.
        void stopPAHProcess();
        // Process the results of WCS from the solving process. If the results are good, we continue to the next phase.
        // Otherwise, we abort the operation.
        void setWCSToggled(bool result);
        // Update GUI to reflect mount status.
        void setMountStatus(ISD::Mount::Status newState);
        // Update the correction offset by this percentage in order to move the triangle around when clicking on
        // for example.
        void setPAHCorrectionOffsetPercentage(double dx, double dy);
        // Update the PAH refresh duration
        void setPAHRefreshDuration(double value)
        {
            pAHExposure->setValue(value);
        }
        // Start the refresh process.
        void startPAHRefreshProcess();
        // This should be called when manual slewing is complete.
        void setPAHSlewDone();
        // Return current active stage label
        QString getPAHStageString(bool translated = true) const
        {
            return translated ? i18n(PAHStages[m_PAHStage]) : PAHStages[m_PAHStage];
        }
        // Return last message
        QString getPAHMessage() const;
        // Set image data from align class
        void setImageData(const QSharedPointer<FITSData> &image);

        void setPAHRefreshAlgorithm(RefreshAlgorithm value);

    protected:
        // Polar Alignment Helper slots
        void rotatePAH();
        void setPAHCorrectionOffset(int x, int y);

    private:
        /**
            * @brief Warns the user if the polar alignment might cross the meridian.
            */
        bool checkPAHForMeridianCrossing();

        /**
             * @brief calculatePAHError Calculate polar alignment error in the Polar Alignment Helper (PAH) method
             * @return True if calculation is successsful, false otherwise.
             */
        bool calculatePAHError();

        /**
         * @brief syncCorrectionVector Flip correction vector based on user settings.
         */
        void syncCorrectionVector();

        /**
         * @brief setupCorrectionGraphics Update align view correction graphics.
         * @param pixel
         */
        void setupCorrectionGraphics(const QPointF &pixel);

        /**
         * @brief supdateRefreshDisplay Updates the UI's refresh error stats.
         * @param azError the azimuth error in degrees
         * @param altError the altitude error in degrees
         */
        void updateRefreshDisplay(double azError, double altError);

    signals:
        // Report new log
        void newLog(const QString &);
        // Request new capture and solve
        void captureAndSolve();
        // Report correction vector and original errors
        void polarResultUpdated(QLineF correctionVector, double polarError, double azError, double altError);
        // Report updated errors
        void updatedErrorsChanged(double total, double az, double alt);
        // Report new correction vector
        void newCorrectionVector(QLineF correctionVector);
        // Report new PAH stage
        void newPAHStage(Stage stage);
        // Report new PAH message
        void newPAHMessage(const QString &message);
        // Report whether the tool is enabled or not
        void PAHEnabled(bool);
        // Request to set alignment table result
        void newAlignTableResult(Align::AlignResult result);
        // Report that the align view was updated.
        void newFrame(const QSharedPointer<FITSView> &view);

    private:
        void updateDisplay(Stage stage, const QString &message);
        void drawArrows(double altError, double azError);
        void showUpdatedError(bool show);
        // These are only used in the plate-solve refresh scheme.
        void solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void startSolver();
        void updatePlateSolveTriangle(const QSharedPointer<FITSData> &image);

        // Polar Alignment Helper
        Stage m_PAHStage { PAH_IDLE };

        SkyPoint targetPAH;

        // Which hemisphere are we located on?
        HemisphereType hemisphere;

        // Polar alignment will retry capture & solve a few times if solve fails.
        int m_PAHRetrySolveCounter { 0 };

        // Points on the image to correct mount's ra axis.
        // correctionFrom is the star the user selected (or center of the image at start).
        // correctionTo is where theuser should move that star.
        // correctionAltTo is where the use should move that star to only fix altitude.
        QPointF correctionFrom, correctionTo, correctionAltTo;

        // RA/DEC coordinates where the image center needs to be move to to correct the RA axis.
        SkyPoint refreshSolution, altOnlyRefreshSolution;


        bool detectStarsPAHRefresh(QList<Edge> *stars, int num, int x, int y, int *xyIndex);

        // Incremented every time sufficient # of stars are detected (for move-star refresh) or
        // when solver is successful (for plate-solve refresh).
        int refreshIteration { 0 };
        // Incremented on every image received.
        int imageNumber { 0 };
        StarCorrespondence starCorrespondencePAH;

        // Class used to estimate alignment error.
        PolarAlign polarAlign;

        // Pointer to image data
        QSharedPointer<FITSData> m_ImageData;

        // Reference to parent
        Align *m_AlignInstance {nullptr};

        // Reference to current active telescope
        ISD::Mount *m_CurrentTelescope { nullptr };

        // Reference to align view
        QSharedPointer<AlignView> m_AlignView;

        // PAH Stage Map
        static const QMap<Stage, const char *> PAHStages;

        // Threshold to stop PAH rotation in degrees
        static constexpr uint8_t PAH_ROTATION_THRESHOLD { 5 };

        PolarAlignWidget *polarAlignWidget {nullptr};

        // Used in the refresh part of polar alignment.
        QSharedPointer<SolverUtils> m_Solver;
        double m_LastRa {0};
        double m_LastDec {0};
        double m_LastOrientation {0};
        double m_LastPixscale {0};

        // Restricts (the internal solver) to using the index and healpix
        // from the previous solve, if that solve was successful.
        int m_IndexToUse { -1 };
        int m_HealpixToUse { -1 };
        int m_NumHealpixFailures { 0 };
};
}
