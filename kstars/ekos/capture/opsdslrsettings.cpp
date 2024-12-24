/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsdslrsettings.h"
#include "kstars.h"
#include "kstarsdata.h"
#include <KConfigDialog>

namespace Ekos
{
OpsDslrSettings::OpsDslrSettings() : QWidget(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("capturesettings");

    connect(clearDSLRInfoB, &QPushButton::clicked, [ = ] ()
    {
        KStarsData::Instance()->userdb()->DeleteAllDSLRInfo();
    });

    connect(m_ConfigDialog, &KConfigDialog::settingsChanged, this, &OpsDslrSettings::settingsUpdated);
}
}
