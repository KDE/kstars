/***************************************************************************
                          opsadvanced.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 14 Mar 2004
    copyright            : (C) 2004 by Jason Harris
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

#ifndef OPSADVANCED_H_
#define OPSADVANCED_H_

#include "ui_opsadvanced.h"

/**@class OpsAdvanced
	*The Advanced Tab of the Options window.  In this Tab the user can configure
	*advanced behaviors of the program, including:
	*@li Whether some objects are hidden when the map is moving (and which objects)
	*@li Whether positions are corrected for atmospheric refraction
	*@li Whether a slewing animation is used to move the Focus position
	*@li Whether centered objects are automatically labeled
	*@li whether a "transient" label is attached when the mouse "hovers" at an object.
	*@author Jason Harris
	*@version 1.0
	*/

class KStars;

class OpsAdvanced : public QFrame, public Ui::OpsAdvanced
{
    Q_OBJECT

public:
    explicit OpsAdvanced( KStars *_ks );
    ~OpsAdvanced();

private slots:
    void slotChangeTimeScale( float newScale );
    void slotToggleHideOptions();

private:
    KStars *ksw;
};

#endif  //OPSADVANCED_H_

