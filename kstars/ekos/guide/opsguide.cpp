/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

#include "Options.h"
#include "opsguide.h"
#include "kstars.h"

#include "internalguide/internalguider.h"

namespace Ekos
{

OpsGuide::OpsGuide()  : QFrame( KStars::Instance() )
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "guidesettings" );

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );    

    guiderTypeButtonGroup->setId(internalGuideR, Guide::GUIDE_INTERNAL);
    guiderTypeButtonGroup->setId(PHD2GuideR, Guide::GUIDE_PHD2);
    guiderTypeButtonGroup->setId(LinGuiderR, Guide::GUIDE_LINGUIDER);

    connect(guiderTypeButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(slotLoadSettings(int)));
}

OpsGuide::~OpsGuide() {}

void OpsGuide::showEvent(QShowEvent *)
{
    slotLoadSettings(static_cast<Guide::GuiderType>(Options::guiderType()));
}

void OpsGuide::slotLoadSettings(Guide::GuiderType guiderType)
{
    switch (guiderType)
    {
    case Guide::GUIDE_INTERNAL:
        internalGuideR->setChecked(true);
        externalHost->clear();
        externalPort->clear();
        externalHost->setEnabled(false);
        externalPort->setEnabled(false);
        break;

    case Guide::GUIDE_PHD2:
        PHD2GuideR->setChecked(true);
        externalHost->setEnabled(true);
        externalPort->setEnabled(true);
        externalHost->setText(Options::pHD2Host());
        externalPort->setText(QString::number(Options::pHD2Port()));
        break;

    case Guide::GUIDE_LINGUIDER:
        LinGuiderR->setChecked(true);
        externalHost->setEnabled(true);
        externalPort->setEnabled(true);
        externalHost->setText(Options::lINGuiderHost());
        externalPort->setText(QString::number(Options::lINGuiderPort()));
        break;
    }        
}

void OpsGuide::slotApply()
{
    Guide::GuiderType type;

    switch (guiderTypeButtonGroup->checkedId())
    {
    case Guide::GUIDE_INTERNAL:
        type = Guide::GUIDE_INTERNAL;
        Options::setGuiderType(Guide::GUIDE_INTERNAL);        
        break;

    case Guide::GUIDE_PHD2:
        type = Guide::GUIDE_PHD2;
        Options::setGuiderType(Guide::GUIDE_PHD2);
        Options::setPHD2Host(externalHost->text());
        Options::setPHD2Port(externalPort->text().toInt());        
        break;

    case Guide::GUIDE_LINGUIDER:
        type = Guide::GUIDE_LINGUIDER;
        Options::setGuiderType(Guide::GUIDE_LINGUIDER);
        Options::setLINGuiderHost(externalHost->text());
        Options::setLINGuiderPort(externalPort->text().toInt());        
        break;
    }

    emit guiderTypeChanged(type);
}

}
