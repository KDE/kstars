/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjects/supernova.h"
#include "supernovaecomponent.h"
#include "notifyupdatesui.h"
#include "ui_notifyupdatesui.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "QTreeWidget"

#include <QApplication>

NotifyUpdatesUI::NotifyUpdatesUI(QWidget *parent) : QDialog(parent), ui(new Ui::NotifyUpdatesUI)
{
    ui->setupUi(this);
    setWindowTitle(i18nc("@title:window", "New Supernova(e) Discovered"));
    connect(ui->centrePushButton, SIGNAL(clicked()), SLOT(slotCenter()));
}

NotifyUpdatesUI::~NotifyUpdatesUI()
{
    delete ui;
}

void NotifyUpdatesUI::addItems(QList<SkyObject *> updatesList)
{
    //int len = updatesList.size();
    foreach (SkyObject *so, updatesList)
    {
        Supernova *sup = (Supernova *)so;

        QString name       = sup->name();
        QString hostGalaxy = i18n("Host Galaxy :: %1", sup->getHostGalaxy());
        QString magnitude  = i18n("Magnitude :: %1", QString::number(sup->getMagnitude()));
        QString type       = i18n("Type :: %1", sup->getType());
        QString position =
            i18n("Position :: RA : %1 Dec : %2", sup->getRA().toHMSString(), sup->getDec().toDMSString());
        QString date = i18n("Date :: %1", sup->getDate());

        QTreeWidgetItem *info    = new QTreeWidgetItem(ui->infoTreeWidget);
        QTreeWidgetItem *hGalaxy = new QTreeWidgetItem(info);
        QTreeWidgetItem *t       = new QTreeWidgetItem(info);
        QTreeWidgetItem *mag     = new QTreeWidgetItem(info);
        QTreeWidgetItem *pos     = new QTreeWidgetItem(info);
        QTreeWidgetItem *dt      = new QTreeWidgetItem(info);

        info->setText(0, name);
        hGalaxy->setText(0, hostGalaxy);
        pos->setText(0, position);
        mag->setText(0, magnitude);
        t->setText(0, type);
        dt->setText(0, date);
        ui->infoTreeWidget->addTopLevelItem(info);
    }
}

void NotifyUpdatesUI::slotCenter()
{
    KStars *kstars = KStars::Instance();
    SkyObject *o   = 0;
    // get selected item
    if (ui->infoTreeWidget->currentItem() != 0)
    {
        if (ui->infoTreeWidget->currentItem()->childCount() > 0) //Serial No. is selected
            o = kstars->data()->objectNamed(ui->infoTreeWidget->currentItem()->text(0));
        else
            o = kstars->data()->objectNamed(ui->infoTreeWidget->currentItem()->parent()->text(0));
    }
    if (o != 0)
    {
        kstars->map()->setFocusPoint(o);
        kstars->map()->setFocusObject(o);
        kstars->map()->setDestination(*kstars->map()->focusPoint());
    }
}
