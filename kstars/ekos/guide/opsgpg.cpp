/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsgpg.h"

#include "guide.h"
#include "kstars.h"
#include "Options.h"
#include "internalguide/internalguider.h"

#include <QFrame>

#include <KConfigDialog>

namespace Ekos
{
OpsGPG::OpsGPG(InternalGuider *guiderObject) : QFrame(KStars::Instance())
{
    setupUi(this);

    guider = guiderObject;

    m_ConfigDialog = KConfigDialog::exists("guidesettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

}

void OpsGPG::slotApply()
{
    guider->updateGPGParameters();
}
}
