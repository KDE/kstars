/*  Custom Properties
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>

#include "indi/indiccd.h"

#include "ui_customproperties.h"

class CustomProperties : public QDialog, public Ui::CustomProperties
{
    Q_OBJECT

public:
    CustomProperties();

    void setCCD(ISD::CCD *ccd);

    QMap<QString, QMap<QString, double> > getCustomProperties() const;
    void setCustomProperties(const QMap<QString, QMap<QString, double> > &value);

signals:
    void valueChanged();

private slots:
    void slotAdd();
    void slotRemove();
    void slotClear();
    void slotApply();

private:

    void syncProperties();

    ISD::CCD *currentCCD = { nullptr };
    QMap<QString, QMap<QString,double>> customProperties;
};
