/*  GPG Options
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
