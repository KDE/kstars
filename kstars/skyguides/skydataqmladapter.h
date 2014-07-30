/***************************************************************************
                          kstarsdataqmladapter.h  -  K Desktop Planetarium
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

#ifndef KSTARSDATAQMLADAPTER_H
#define KSTARSDATAQMLADAPTER_H

#include <QObject>

class SkyObject;

class SkyDataQmlAdapter : public QObject
{
	Q_OBJECT

public:
	SkyDataQmlAdapter(QObject * parent = 0);
	~SkyDataQmlAdapter();

	static SkyObject * findSkyObjectByName(const QString & name);

	Q_INVOKABLE double getSkyObjectRightAscention(const QString & name);
	Q_INVOKABLE double getSkyObjectDeclination(const QString & name);
	Q_INVOKABLE float getSkyObjectMagnitude(const QString & name);

private:
	const SkyObject * preloadedObject;

	bool preloadObject(const QString & name);
};

#endif // KSTARSDATAQMLADAPTER_H
