/***************************************************************************
                          skyguideslistview.cpp  -  K Desktop Planetarium
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

#include <QDeclarativeContext>
#include <QVariant>
#include <QDesktopWidget>
#include <QRect>

#include <kstandarddirs.h>
#include <kdeclarative.h>

#include "skyguideslistview.h"

#include "skyguideslistmodel.h"
#include "../skymap.h"


SkyGuidesListView::SkyGuidesListView(const SkyGuidesListModel & guidesModel, SkyMap * skyMap) :
	QDeclarativeView(0),
	mSkyMapQmlAdapter(skyMap),
	mKStarsDataQmlAdapter(this)
{
	KDeclarative kd;
	QString qmlpath = KStandardDirs::locate("appdata","skyguides/qml/GuidesListView.qml");

	kd.setDeclarativeEngine(engine()); 	///To use i18n() instead of qsTr() in qml/wiview.qml for translation
	kd.initialize();
	kd.setupBindings();

	rootContext()->setContextProperty( "guidesmodel", QVariant::fromValue(guidesModel.data()) );
	rootContext()->setContextProperty( "skymap", &mSkyMapQmlAdapter );
	rootContext()->setContextProperty( "skydata", &mKStarsDataQmlAdapter );
	rootContext()->setContextProperty( "widget", this );

	setMinimumSize(210, 400);
	setResizeMode(QDeclarativeView::SizeRootObjectToView);
	setSource(qmlpath);
}

void SkyGuidesListView::resizeScene(uint width, uint height)
{
	resize( width, height );
}

int SkyGuidesListView::getScreenWidth()
{
	QDesktopWidget desk;
	return desk.availableGeometry(desk.primaryScreen()).width();
}

int SkyGuidesListView::getScreenHeight()
{
	QDesktopWidget desk;
	return desk.availableGeometry(desk.primaryScreen()).height();
}

SkyGuidesListView::~SkyGuidesListView()
{}