/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsfits.h"
#include "config-kstars.h"

#include <QStandardItemModel>
#include <KConfigDialog>

#ifdef HAVE_STELLARSOLVER
#include "ekos/auxiliary/stellarsolverprofileeditor.h"

namespace SSolver
{
class Parameters;
}
#endif

class KStars;

/** @class OpsFITS
 *The FITS Tab of the Options window.  Configure FITS options including look and feel and how FITS Viewer processes the data.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class OpsFITS : public QFrame, public Ui::OpsFITS
{
        Q_OBJECT

    public:
        explicit OpsFITS();

    private:
#ifdef HAVE_STELLARSOLVER
        // Initializes the HFR options menu.
        void setupHFROptions();
        void loadStellarSolverProfiles();

        QList<SSolver::Parameters> m_StellarSolverProfiles;
        QString savedOptionsProfiles;
        Ekos::StellarSolverProfileEditor *optionsProfileEditor { nullptr };
#endif

};

