/***************************************************************************
                          opsxplanet.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 26 Nov 2008
    copyright            : (C) 2008 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OPSXPLANET_H_
#define OPSXPLANET_H_

#include "ui_opsxplanet.h"

class KStars;

class OpsXplanet : public QFrame, public Ui::OpsXplanet
{
    Q_OBJECT

public:
    explicit OpsXplanet( KStars *_ks );
    ~OpsXplanet();

private:
    KStars *ksw;

private slots:
    void slotUpdateWidgets( bool on );
    void slotConfigFileWidgets( bool on );
    void slotStarmapFileWidgets( bool on );
    void slotArcFileWidgets( bool on );
    void slotLabelWidgets( bool on );
    void slotMarkerFileWidgets( bool on );
    void slotMarkerBoundsWidgets( bool on );
    void slotProjectionWidgets( int index );
    void slotBackgroundWidgets( bool on );
    void toggleXPlanetInternal();
};

#endif // OPSGUIDES_H_
