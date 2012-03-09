/***************************************************************************
                          notifyupdatesui.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/05/03
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "skyobjects/supernova.h"
#include "supernovaecomponent.h"
#include "notifyupdatesui.h"
#include "ui_notifyupdatesui.h"
#include "kdebug.h"
#include "QTreeWidget"
#include <QtGui/QApplication>

NotifyUpdatesUI::NotifyUpdatesUI(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NotifyUpdatesUI)
{
    ui->setupUi(this);
}

NotifyUpdatesUI::~NotifyUpdatesUI()
{
    delete ui;
}

void NotifyUpdatesUI::addItems(QList<SkyObject*> updatesList)
{
    //int len = updatesList.size();
    foreach (SkyObject *so , updatesList)
    {
        Supernova * sup = (Supernova *)so;

        QString name = sup->name();
        QString hostGalaxy = "Host Galaxy :: ";
        hostGalaxy.append(sup->getHostGalaxy());
        QString magnitude = "Magnitude :: ";
        magnitude.append(QString::number(sup->getMagnitude()));
        QString type = "Type :: ";
        type.append(sup->getType());
        QString position = "Position :: RA : ";
        position.append(sup->getRA().toHMSString());
        position.append(" Dec : ");
        position.append(sup->getDec().toDMSString());
        QString date = "Date :: ";
        date.append(sup->getDate());

        QTreeWidgetItem *info = new QTreeWidgetItem(ui->infoTreeWidget);
        QTreeWidgetItem *hGalaxy = new QTreeWidgetItem(info);
        QTreeWidgetItem *t = new QTreeWidgetItem(info);
        QTreeWidgetItem *mag = new QTreeWidgetItem(info);
        QTreeWidgetItem *pos = new QTreeWidgetItem(info);
        QTreeWidgetItem *dt = new QTreeWidgetItem(info);

        info->setText(0,  name);
        hGalaxy->setText(0, hostGalaxy);
        pos->setText(0, position);
        mag->setText(0, magnitude);
        t->setText(0, type);
        dt->setText(0, date);
        ui->infoTreeWidget->addTopLevelItem(info);
    }
}
