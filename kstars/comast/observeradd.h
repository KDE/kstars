/***************************************************************************
                          observeradd.h  -  description

                             -------------------
    begin                : Sunday July 26, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OBSERVERADD_H_
#define OBSERVERADD_H_

#include "ui_observeradd.h"

#include <QWidget>
#include <kdialog.h>

#include "kstars.h"

class ObserverAdd : public KDialog {
Q_OBJECT
    public:
        ObserverAdd();
        void loadObservers();
        void saveObservers();

    public slots:
        void slotAddObserver();

    private:
        KStars *ks;
        Ui::ObserverAdd ui;
};
#endif
