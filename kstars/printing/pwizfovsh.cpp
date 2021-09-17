/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
