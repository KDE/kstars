/***************************************************************************
                          pwizfovsh.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Aug 15 2011
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

#include "pwizfovsh.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "pwizobjectselection.h"
#include "printingwizard.h"
#include "dialogs/finddialog.h"
#include "dialogs/detaildialog.h"

#include <QPointer>

PWizFovShUI::PWizFovShUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    setupWidgets();
    setupConnections();
}

void PWizFovShUI::setBeginObject(SkyObject *obj)
{
    m_ParentWizard->setShBeginObject(obj);
    objInfoLabel->setText(PWizObjectSelectionUI::objectInfoString(obj));
    objInfoLabel->setVisible(true);
    detailsButton->setVisible(true);
    captureButton->setEnabled(true);
}

void PWizFovShUI::slotSelectFromList()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *obj = FindDialog::Instance()->targetObject();
        if (obj)
        {
            setBeginObject(obj);
        }
    }
}

void PWizFovShUI::slotPointObject()
{
    m_ParentWizard->beginShBeginPointing();
}

void PWizFovShUI::slotDetails()
{
    if (m_ParentWizard->getShBeginObject())
    {
        QPointer<DetailDialog> detailDlg(new DetailDialog(m_ParentWizard->getShBeginObject(),
                                                          KStars::Instance()->data()->ut(),
                                                          KStars::Instance()->data()->geo(), this));
        detailDlg->exec();
        delete detailDlg;
    }
}

void PWizFovShUI::slotBeginCapture()
{
    m_ParentWizard->beginShFovCapture();
}

void PWizFovShUI::setupWidgets()
{
    QStringList fovNames;
    foreach (FOV *fov, KStarsData::Instance()->getAvailableFOVs())
    {
        fovNames.append(fov->name());
    }
    fovCombo->addItems(fovNames);

    objInfoLabel->setVisible(false);
    detailsButton->setVisible(false);
    captureButton->setEnabled(false);
}

void PWizFovShUI::setupConnections()
{
    connect(selectFromListButton, SIGNAL(clicked()), this, SLOT(slotSelectFromList()));
    connect(pointButton, SIGNAL(clicked()), this, SLOT(slotPointObject()));
    connect(captureButton, SIGNAL(clicked()), this, SLOT(slotBeginCapture()));
    connect(detailsButton, SIGNAL(clicked()), this, SLOT(slotDetails()));
}
