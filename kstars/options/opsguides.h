/***************************************************************************
                          opsguides.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 6 Feb 2005
    copyright            : (C) 2005 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OPSGUIDES_H_
#define OPSGUIDES_H_

#include "ui_opsguides.h"

class KStars;

class OpsGuides : public QFrame, public Ui::OpsGuides
{
    Q_OBJECT

public:
    explicit OpsGuides( KStars *_ks );
    ~OpsGuides();

private slots:
    void slotToggleConstellOptions( bool state );
    void slotToggleMilkyWayOptions( bool state );
    void slotToggleOpaqueGround( bool state );
    void slotToggleAutoSelectGrid( bool state );

private:
    KStars *ksw;
};

#endif // OPSGUIDES_H_
