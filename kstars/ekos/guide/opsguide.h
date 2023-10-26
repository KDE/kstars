/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsguide.h"

#include "guide.h"
#include "stellarsolver.h"
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

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
        QList<SSolver::Parameters> optionsList;
        StellarSolverProfileEditor *optionsProfileEditor { nullptr };
};
}
