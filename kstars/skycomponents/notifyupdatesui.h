/***************************************************************************
                          notifyupdatesui.h  -  K Desktop Planetarium
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

