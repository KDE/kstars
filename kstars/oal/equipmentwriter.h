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
#include <QDialog>

#include "kstars.h"

class KStars;

class EquipmentWriter : public QDialog
{
    Q_OBJECT
  public:
    EquipmentWriter();
    void saveEquipment();
    void loadEquipment();

  public slots:
    void slotAddScope();
    void slotAddEyepiece();
    void slotAddLens();
    void slotAddFilter();
    void slotSaveScope();
    void slotSaveEyepiece();
    void slotSaveLens();
    void slotSaveFilter();
    void slotRemoveScope();
    void slotRemoveEyepiece();
    void slotRemoveLens();
    void slotRemoveFilter();
    void slotSetScope(QString);
    void slotSetEyepiece(QString);
    void slotSetLens(QString);
    void slotSetFilter(QString);
    void slotNewScope();
    void slotNewEyepiece();
    void slotNewLens();
    void slotNewFilter();
    void slotClose();
    void slotSave();

  private:
    Ui::EquipmentWriter ui;
    bool newScope, newEyepiece, newLens, newFilter;
    int nextScope, nextEyepiece, nextLens, nextFilter;
};

#endif
