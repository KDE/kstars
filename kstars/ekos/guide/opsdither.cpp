/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

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
