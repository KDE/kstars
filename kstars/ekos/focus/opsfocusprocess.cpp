/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsfocusprocess.h"
#include "kstars.h"
#include <KConfigDialog>

namespace Ekos
{
OpsFocusProcess::OpsFocusProcess() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("focussettings");

    connect(m_ConfigDialog, SIGNAL(settingsChanged(QString)), this, SIGNAL(settingsUpdated()));
}
}
