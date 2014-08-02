/***************************************************************************
                          skymapqmladapter.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/04/07
    copyright            : (C) 2014 by Gioacchino Mazzurco
    email                : gmazzurco89@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skymapqmladapter.h"
#include "skymap.h"
#include "dms.h"
#include "skydataqmladapter.h"

SkyMapQmlAdapter::SkyMapQmlAdapter(SkyMap * skymap, QObject * parent) :
	QObject(parent),
	mSkyMap(skymap)
{}

void SkyMapQmlAdapter::setFocusByCoordinates( double ra, double dec ) const
{
	dms dRa(ra);
	dms dDec(dec);
	mSkyMap->setFocus( dRa, dDec );
}

void SkyMapQmlAdapter::setFocusBySkyObjectName(const QString & name) const
{
	SkyObject * obj = SkyDataQmlAdapter::findSkyObjectByName(name);
	if(obj)
		mSkyMap->setFocus(obj);
}

void SkyMapQmlAdapter::setDestinationByCoordinates( double ra, double dec ) const
{
	dms dRa(ra);
	dms dDec(dec);
	mSkyMap->setDestination( dRa, dDec );
}

void SkyMapQmlAdapter::setDestinationBySkyObjectName(const QString & name) const
{
	SkyObject * obj = SkyDataQmlAdapter::findSkyObjectByName(name);
	if(obj)
		mSkyMap->setDestination(*obj);
}
