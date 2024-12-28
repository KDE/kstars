/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsfocusmechanics.h"
#include <KConfigDialog>

namespace Ekos
{
OpsFocusMechanics::OpsFocusMechanics(const QString name) : OpsFocusBase(name)
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists(dialogName());
    if (m_ConfigDialog)
        connect(m_ConfigDialog, &KConfigDialog::settingsChanged, this, &OpsFocusMechanics::settingsUpdated);
}
}
