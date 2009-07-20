/***************************************************************************
                          equipmentwriter.h  -  description

                             -------------------
    begin                : Friday July 19, 2009
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

#ifndef EQUIPMENTWRITER_H_
#define EQUIPMENTWRITER_H_

#include "ui_equipmentwriter.h"

#include <QWidget>
#include <kdialog.h>

#include "kstars.h"

class KStars;

class EquipmentWriter : public KDialog {
Q_OBJECT
    public:
        EquipmentWriter();

    public slots:
        void slotAddScope();
        void slotAddEyepiece();
        void slotAddLens();
        void slotAddFilter();
        void slotClose() { hide(); }
        void slotSaveEquipment();
        void slotLoadEquipment();

    private:
        KStars *ks;
        Ui::EquipmentWriter ui;

};

#endif
