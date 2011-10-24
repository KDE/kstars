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

#include <kdialog.h>

#include "oal.h"

class KStars;

class EquipmentWriter : public KDialog
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
    void slotSetScope(int idx);
    void slotSetEyepiece(int idx);
    void slotSetLens(int idx);
    void slotSetFilter(int idx);
    void slotClose();

private slots:
    void slotLightGraspDefined(bool enabled);
    void slotOrientationDefined(bool enabled);
    void slotFovDefined(bool enabled);
    void slotZoomEyepieceDefined(bool enabled);

private:
    void setupFilterTab();

    void clearScopePage();
    void clearEyepiecePage();
    void clearLensPage();
    void clearFilterPage();

    KStars *m_Ks;
    OAL::Log *m_LogObject;
    Ui::EquipmentWriter m_Ui;
    int nextLens, nextFilter;
};

#endif
