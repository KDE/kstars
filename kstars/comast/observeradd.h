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
        //The default constructor
        ObserverAdd();

        /*@short function to load the list of
         * observers from the file
         */
        void loadObservers();

        /*@short function to save the list of
         * observers to the file
         */
        void saveObservers();

    public slots:
        /*@short function to add the new observer
         * to the observerList of the global logObject
         */
        void slotAddObserver();

    private:
        KStars *ks;
        Ui::ObserverAdd ui;
        int nextObserver;
};
#endif
