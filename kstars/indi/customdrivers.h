/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QVariantMap>
#include <QSqlTableModel>
#include <QPointer>

#include "ui_customdrivers.h"

class DriverInfo;

/**
 * @class CustomDrivers
 * @short Handles adding new drivers to database. This would enable to add arbitrary aliases of existing drivers.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class CustomDrivers : public QDialog, public Ui::CustomDrivers
{
    Q_OBJECT

  public:
    explicit CustomDrivers(QWidget *parent, const QList<DriverInfo *> &driversList);
    ~CustomDrivers();

    const QList<QVariantMap> & customDrivers() const { return m_CustomDrivers; }
    void refreshFromDB();

  protected slots:
    void syncDriver();
    void addDriver();
    void removeDriver();

  private:            

    QList<QVariantMap> m_CustomDrivers;
    const QList<DriverInfo *> &m_DriversList;

    QSqlDatabase userdb;
    QPointer<QSqlTableModel> model;
};

