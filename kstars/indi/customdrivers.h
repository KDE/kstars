/*  Driver Alias
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

