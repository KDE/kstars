/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsguide.h"

#include "guide.h"
#include "stellarsolver.h"
#undef Const
#include "parameters.h"

class KConfigDialog;

namespace Ekos
{

class StellarSolverProfileEditor;

/**
 * @class OpsGuide
 *
 * Enables the user to set guide options
 *
 * @author Jasem Mutlaq
 */
class OpsGuide : public QFrame, public Ui::OpsGuide
{
        Q_OBJECT

    public:
        explicit OpsGuide();
        virtual ~OpsGuide() override = default;
        void loadOptionsProfiles();

        // Needs to be same as options for RAGuidePulseAlgorithm and
        // DECGuidePulseAlgorithm in opsguide.ui
        enum GuidePulseAlgorithm
        {
            STANDARD_ALGORITHM = 0,
            HYSTERESIS_ALGORITHM,
            LINEAR_ALGORITHM,
            GPG_ALGORITHM
        };

    signals:
        void settingsUpdated();

    private slots:
        void setRAGuidePulseAlg(int index);
      void setDECGuidePulseAlg(int index);

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
        QList<SSolver::Parameters> optionsList;
        StellarSolverProfileEditor *optionsProfileEditor { nullptr };
};
}
