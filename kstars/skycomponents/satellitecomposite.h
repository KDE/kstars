/***************************************************************************
                          satellitecomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 22 Nov 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SATELLITECOMPOSITE_H
#define SATELLITECOMPOSITE_H

#include <QStringList>
#include <QVector>

#include "skycomposite.h"
extern "C" {
#include "satlib/SatLib.h"
}

class SatelliteComposite : public SkyComposite
{
public:
    /**
    	*@short Constructor
    	*@param parent Pointer to the parent SkyComposite object
    	*/
    SatelliteComposite( SkyComposite *parent );

    ~SatelliteComposite();

    /**@short Update the satellite tracks
     * @param num Pointer to the KSNumbers object
     */
    virtual void update( KSNumbers *num=0 );

private:
    QVector<SPositionSat*> pSat;
    QStringList SatelliteNames;
    long double JD_0;
};

#endif
