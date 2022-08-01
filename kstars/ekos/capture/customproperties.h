/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include "indi/indicamera.h"

#include "ui_customproperties.h"

class CustomProperties : public QDialog, public Ui::CustomProperties
{
        Q_OBJECT

    public:
        CustomProperties();

        void setCCD(ISD::Camera *ccd);

        QMap<QString, QMap<QString, QVariant> > getCustomProperties() const;
        void setCustomProperties(const QMap<QString, QMap<QString, QVariant> > &value);

    signals:
        void valueChanged();

    private slots:
        void slotAdd();
        void slotRemove();
        void slotClear();
        void slotApply();

    private:

        void syncProperties();

        ISD::Camera *currentCCD = { nullptr };
        QMap<QString, QMap<QString, QVariant>> customProperties;
};
