/***************************************************************************
               starhopperdialog.cpp  -  UI of Star Hopping Guide for KStars
                             -------------------
    begin                : Sat 15th Nov 2014
    copyright            : (C) 2014 Utkarsh Simha
    email                : utkarshsimha@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _STARHOPPERDIALOG_H_
#define _STARHOPPERDIALOG_H_

#include "skyobjects/starobject.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "tools/starhopper.h"
#include "skycomponents/targetlistcomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "ksutils.h"

#include "ui_starhopperdialog.h"
#include<QDialog>

Q_DECLARE_METATYPE(StarObject *)

class StarHopperDialog : public QDialog, public Ui::StarHopperDialog {
    Q_OBJECT;
    public:
        StarHopperDialog(QWidget *parent = 0);
        virtual ~StarHopperDialog();
        /**
         *@short Forms a Star Hop route from source to destination and displays on skymap
         *@param startHop SkyPoint to the start of Star Hop
         *@param stopHop SkyPoint to destination of StarHop
         *@param fov Field of view under consideration
         *@param maglim Magnitude limit of star to search for
         *@note In turn calls StarHopper to perform computations
         */
        void starHop( const SkyPoint& startHop, const SkyPoint& stopHop, float fov, float maglim);

    private slots:
        void slotNext();
        void slotGoto();
        void slotDetails();
        void slotRefreshMetadata();

    private:
        SkyObject * getStarData(QListWidgetItem *);
        void setData(StarObject *);
        inline TargetListComponent * getTargetListComponent();
        QList<SkyObject *> *m_skyObjList;
        StarHopper *m_sh;
        Ui::StarHopperDialog *ui;
        QListWidget *m_lw;
        QStringList *m_Metadata;
};
#endif

