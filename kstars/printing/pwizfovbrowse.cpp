/***************************************************************************
                          pwizfovbrowse.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Aug 12 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "pwizfovbrowse.h"

#include <QPointer>
#include "foveditordialog.h"

PWizFovBrowseUI::PWizFovBrowseUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    connect(browseButton, SIGNAL(clicked()), this, SLOT(slotOpenFovEditor()));
}

void PWizFovBrowseUI::slotOpenFovEditor()
{
    QPointer<FovEditorDialog> dialog(new FovEditorDialog(m_ParentWizard, this));
    dialog->exec();
    delete dialog;
}
