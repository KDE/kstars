/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

#include "Options.h"
#include "opsguide.h"
#include "kstars.h"
#include "auxiliary/ksnotification.h"

#include "internalguide/internalguider.h"

namespace Ekos
{
OpsGuide::OpsGuide() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("guidesettings");

    connect(kcfg_GuideRemoteImagesEnabled, &QCheckBox::toggled, this, [this] ()
    {
        if (Options::guideRemoteImagesEnabled() != kcfg_GuideRemoteImagesEnabled->isChecked())
            KSNotification::info(i18n("You must restart KStars for this change to take effect."));
    });

    connect(m_ConfigDialog, SIGNAL(settingsChanged(QString)), this, SIGNAL(settingsUpdated()));
}

OpsGuide::~OpsGuide()
{
}

}
