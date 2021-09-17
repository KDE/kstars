/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsdither.h"

#include "Options.h"
#include "kstars.h"
#include "auxiliary/ksnotification.h"
#include "internalguide/internalguider.h"

#include <KConfigDialog>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStringList>

namespace Ekos
{
OpsDither::OpsDither() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("guidesettings");

    connect(kcfg_DitherNoGuiding, &QCheckBox::toggled, this, [&](bool checked)
    {
        if (checked && kcfg_DitherEnabled->isChecked())
        {
            KSNotification::error("Guided dithering cannot be used along with non-guided dithering.");
            kcfg_DitherEnabled->setChecked(false);
        }
    });

    connect(kcfg_DitherEnabled, &QCheckBox::toggled, this, [&](bool checked)
    {
        if (checked && kcfg_DitherNoGuiding->isChecked())
        {
            KSNotification::error("Guided dithering cannot be used along with non-guided dithering.");
            kcfg_DitherNoGuiding->setChecked(false);
        }
    });

    connect(m_ConfigDialog, SIGNAL(settingsChanged(QString)), this, SIGNAL(settingsUpdated()));

}

}
