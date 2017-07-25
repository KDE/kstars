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

#pragma once

#include "ui_starhopperdialog.h"

#include "starobject.h"

#include <QDialog>

#include <memory>

class SkyObject;
class SkyPoint;
class StarHopper;
class StarObject;
class TargetListComponent;

Q_DECLARE_METATYPE(StarObject *)

class StarHopperDialog : public QDialog, public Ui::StarHopperDialog
{
    Q_OBJECT;

  public:
    explicit StarHopperDialog(QWidget *parent = nullptr);
    virtual ~StarHopperDialog();

    /**
     * @short Forms a Star Hop route from source to destination and displays on skymap
     * @param startHop SkyPoint to the start of Star Hop
     * @param stopHop SkyPoint to destination of StarHop
     * @param fov Field of view under consideration
     * @param maglim Magnitude limit of star to search for
     * @note In turn calls StarHopper to perform computations
     */
    void starHop(const SkyPoint &startHop, const SkyPoint &stopHop, float fov, float maglim);

  private slots:
    void slotNext();
    void slotGoto();
    void slotDetails();
    void slotRefreshMetadata();

  private:
    SkyObject *getStarData(QListWidgetItem *);
    void setData(StarObject *);
    TargetListComponent *getTargetListComponent();

    QList<SkyObject *> *m_skyObjList { nullptr };
    std::unique_ptr<StarHopper> m_sh;
    Ui::StarHopperDialog *ui { nullptr };
    QListWidget *m_lw { nullptr };
    QStringList *m_Metadata { nullptr };
};
