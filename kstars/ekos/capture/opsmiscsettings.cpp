/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsmiscsettings.h"
#include "kstars.h"
#include <KConfigDialog>

namespace Ekos
{
OpsMiscSettings::OpsMiscSettings() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("capturesettings");

    connect(m_ConfigDialog, &KConfigDialog::settingsChanged, this, &OpsMiscSettings::settingsUpdated);
}
}
