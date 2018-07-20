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

#include "ui_driveralias.h"

class DriverInfo;

/**
 * @class DriverAlias
 * @short Handles adding new drivers to database. This would enable to add arbitrary aliases of existing drivers.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class DriverAlias : public QDialog, public Ui::DriverAlias
{
    Q_OBJECT

  public:
    explicit DriverAlias(QWidget *parent, const QList<DriverInfo *> &driversList);
    ~DriverAlias();

    void refreshFromDB();

  signals:

  public slots:    

  private:            

    QList<QVariantMap> driverAliases;
    const QList<DriverInfo *> &m_DriversList;
};

