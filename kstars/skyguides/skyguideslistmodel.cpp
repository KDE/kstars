/***************************************************************************
                          skyguideslistview.h  -  K Desktop Planetarium
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


#include <QDebug>
#include <QString>
#include <QStringList>
#include <QtAlgorithms>
#include <QFile>
#include <QIODevice>
#include <QByteArray>

#include <kstandarddirs.h>

#include "skyguideslistmodel.h"
#include "skyguidedata.h"
#include "../skyobjects/skyobject.h"


SkyGuidesListModel::SkyGuidesListModel(const SkyObject * obj) :
	mData(),
	actualSkyObject(NULL)
{
	updateSkyObject(obj);
}

SkyGuidesListModel::SkyGuidesListModel(const SkyGuidesListModel & skglm) :
	mData(skglm.mData),
	actualSkyObject(NULL)
{
	updateSkyObject(skglm.actualSkyObject);
};

SkyGuidesListModel::SkyGuidesListModel() :
	mData(),
	actualSkyObject(NULL)
{};

void SkyGuidesListModel::updateSkyObject(const SkyObject * obj)
{
	if (obj)
	{
		if ( actualSkyObject && (!( (*actualSkyObject) == (*obj) )) )
			return;

		KStandardDirs ksd;
		QStringList guidesPaths = ksd.findAllResources("data", "kstars/skyguides/guides/*/index.html");

		qDeleteAll(mData);
		mData.clear();

		foreach( QString path, guidesPaths )
		{
			path.chop(10);

			QStringList gos;
			QFile skOFile(path + "/skyobjects.txt");
			skOFile.open(QIODevice::ReadOnly);
			while(!skOFile.atEnd())
				gos.append(skOFile.readLine().trimmed());
			skOFile.close();

			bool match = false;
			if (obj->hasName())
				match = gos.contains(obj->name(), Qt::CaseInsensitive) || gos.contains(obj->translatedName(), Qt::CaseInsensitive);
			if (!match && obj->hasName2())
				match = gos.contains(obj->name2(), Qt::CaseInsensitive) || gos.contains(obj->translatedName2(), Qt::CaseInsensitive);
			if (!match && obj->hasLongName())
				match |= gos.contains(obj->longname(), Qt::CaseInsensitive) || gos.contains(obj->translatedLongName(), Qt::CaseInsensitive);

			if (match)
			{
				QFile titleFile(path + "/title.txt");
				titleFile.open(QIODevice::ReadOnly);
				QString title(titleFile.readLine().trimmed());
				titleFile.close();

				mData.append(new SkyGuideData(title, path));

				qDebug() << "Found: "<< title << " at " << path;
			}
		}

		delete actualSkyObject;
		actualSkyObject = obj->clone();
	}
}

SkyGuidesListModel::~SkyGuidesListModel()
{
	qDeleteAll(mData);
	delete actualSkyObject;
}

