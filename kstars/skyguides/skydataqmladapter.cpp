/***************************************************************************
                          kstarsdataqmladapter.cpp  -  K Desktop Planetarium
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

#include <QString>

#include "skydataqmladapter.h"

#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "starcomponent.h"
#include "skyobject.h"

SkyDataQmlAdapter::SkyDataQmlAdapter(QObject * parent) :
	QObject(parent),
	preloadedObject(0)
{}

SkyDataQmlAdapter::~SkyDataQmlAdapter()
{
	delete(preloadedObject);
}

double SkyDataQmlAdapter::getSkyObjectRightAscention(const QString & name)
{
	if(preloadObject(name))
		return preloadedObject->ra().Degrees();

	return 0;
}

double SkyDataQmlAdapter::getSkyObjectDeclination(const QString & name)
{
	if(preloadObject(name))
		return preloadedObject->dec().Degrees();

	return 0;
}

float SkyDataQmlAdapter::getSkyObjectMagnitude(const QString & name)
{
	if(preloadObject(name))
		return preloadedObject->mag();

	return 0;
}

SkyObject * SkyDataQmlAdapter::findSkyObjectByName(const QString & name)
{
	SkyObject * foundObject = KStarsData::Instance()->skyComposite()->findByName(name);

	if(!foundObject && name.startsWith("HD"))
	{
		QString hdString(name);
		hdString.remove("HD");

		bool conversionOk;
		uint hd = hdString.toInt(&conversionOk);
		if(conversionOk)
			foundObject = StarComponent::Instance()->findByHDIndex(hd);
	}

	return foundObject;
}

bool SkyDataQmlAdapter::preloadObject(const QString & name)
{
	if (preloadedObject && preloadedObject->hasName() && (preloadedObject->name() == name))
		return true;

	SkyObject * foundObject = findSkyObjectByName(name);
	if (foundObject)
	{
		delete(preloadedObject);
		preloadedObject = foundObject->clone();
		return true;
	}

	return false;
}
