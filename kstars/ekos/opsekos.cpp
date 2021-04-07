/*  Ekos Options
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include "opsekos.h"

#include "manager.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

#include <KConfigDialog>

OpsEkos::OpsEkos() : QTabWidget(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(clearDSLRInfoB, &QPushButton::clicked, [ = ] ()
    {
        KStarsData::Instance()->userdb()->DeleteAllDSLRInfo();
    });

    connect(kcfg_EkosTopIcons, &QRadioButton::toggled, this, [this]()
    {
        if (Options::ekosTopIcons() != kcfg_EkosTopIcons->isChecked())
            KSNotification::info(i18n("You must restart KStars for this change to take effect."));
    });
}
