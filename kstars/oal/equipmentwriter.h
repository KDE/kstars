/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

