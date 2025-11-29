/*  Tool for Push-To support for manual mounts.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>, 2025

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include "ekos/ekos.h"
#include "ui_pushtoassistant.h"

namespace Ekos
{
class PushToAssistant : public QDialog, public Ui::PushToAssistant
{
        Q_OBJECT
    public:
        static PushToAssistant *Instance();
        static void release();

        /**
         * @brief updateTelescopeCoords update current telescope position
         */
        void updateTelescopeCoords(const SkyPoint &position);

        /**
         * @brief enableMountPosition Enable/disable all widgets required for mount position
         */
        void enableMountPosition(bool enable);
        /**
         * @brief enableSolving Enable/disable all widgets required for plagte solving
         */
        void enableSolving(bool enable);
        /**
         * @brief handleEkosStart Change status line depending whether EKOS is running
         */
        void handleEkosStatus(CommunicationStatus status);

        // set target position target name
        /**
         * @brief setTargetPosition set the target position, coordinates in JNow
         */
        void setTargetPosition(SkyPoint *position, bool updateTargetName = false);
        void setTargetName(const QString &name)
        {
            mountTarget->setTargetName(name);
        }

        /**
        * @brief updateSolution Slot for handling a new solution sent from Align
        */
        void updateSolution(const QVariantMap &solution);

        // set the switch between J2000 and JNow
        void setJ2000Enabled(bool enabled)
        {
            mountTarget->setJ2000Enabled(enabled);
        }

        /**
         * @brief setAlignState handle alignment state
         */
        void setAlignState(AlignState state);

    signals:
        void sync(double RA, double DE);
        void abort();
        void captureAndSolve(bool initialCall = true);

    private:
        PushToAssistant(QWidget *parent = nullptr);
        ~PushToAssistant() override;

        /**
         * @brief initStatusLine initialisation of the status line at the bottom of the assistant
         */
        void initStatusLine();

        /**
         * @brief updateTargetError calculate the difference in altitude and azimuth between
         *        current mount position ans the target we are aiming for
         */
        void updateTargetError();

        /**
         * @brief togglePlatesolving We use the plate solve button both for starting and stopping
         *        plate solving - dependent on the align module state.
         */
        void toggleSolving();

        /**
         * @brief repeatSolving Turn on an off repeated solving
         */
        void repeatSolving(bool repeat);

        /**
         * @brief updateTogglePlatesolvingButton Update text and tool tip depending on the
         *        plate solving state
         * @param platesolve if true the button should be used for plate solving, otherwise
         *        for stopping the plate solver
         */
        void updateToggleSolvingB(bool platesolve);

        AlignState m_alignState = ALIGN_IDLE;

        inline static QPointer<PushToAssistant> _PushToAssistant = nullptr;

        // current scope position
        SkyPoint scopePosition;
        // Status LEDs
        QPointer<KLed> alignStatusLed, mountStatusLed;
        // timer for repeated solving
        QTimer solvingTimer;

};
}; // namespace
