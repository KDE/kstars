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
	///To use i18n() instead of qsTr() in qml/wiview.qml for translation
	KDeclarative kd;
	kd.setDeclarativeEngine(engine());
	kd.initialize();
	kd.setupBindings();

	rootContext()->setContextProperty( "guidesmodel", QVariant::fromValue(guidesModel.data()) );
	rootContext()->setContextProperty( "skymap", &mSkyMapQmlAdapter );
	rootContext()->setContextProperty( "skydata", &mKStarsDataQmlAdapter );
	rootContext()->setContextProperty( "widget", this );

	QString qmlpath = KStandardDirs::locate("appdata","skyguides/qml/GuidesListView.qml");
	setSource(qmlpath);
	setResizeMode(QDeclarativeView::SizeViewToRootObject);
}

void SkyGuidesListView::showNormal()
{
	setResizeMode(QDeclarativeView::SizeViewToRootObject);
	QDeclarativeView::showNormal();
}

void SkyGuidesListView::showFullScreen()
{
	setResizeMode(QDeclarativeView::SizeRootObjectToView);
	QDeclarativeView::showFullScreen();
}

void SkyGuidesListView::showMaximized()
{
	setResizeMode(QDeclarativeView::SizeRootObjectToView);
	QDeclarativeView::showFullScreen();
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