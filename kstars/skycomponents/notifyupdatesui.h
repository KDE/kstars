/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyobjects/skyobject.h"

#include <QDialog>

namespace Ui
{
class NotifyUpdatesUI;
}

class NotifyUpdatesUI : public QDialog
{
        Q_OBJECT

    public:
        explicit NotifyUpdatesUI(QWidget *parent = 0);
        ~NotifyUpdatesUI();
        void addItems(QList<SkyObject *> updatesList);

    private slots:
        void slotCenter();

    private:
        Ui::NotifyUpdatesUI *ui;
};

