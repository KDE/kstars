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

#include <QWidget>
#include <QDeclarativeContext>
#include <QVariant>

#include <kstandarddirs.h>
#include <kdeclarative.h>

#include "skyguideslistview.h"
#include "skyguideslistmodel.h"


SkyGuidesListView::SkyGuidesListView(const SkyGuidesListModel & guidesModel, QWidget * parent) :
	QWidget(parent),
	mBaseView()
{
	///To use i18n() instead of qsTr() in qml/wiview.qml for translation
	KDeclarative kd;
	kd.setDeclarativeEngine(mBaseView.engine());
	kd.initialize();
	kd.setupBindings();

	mBaseView.rootContext()->setContextProperty( "guidesModel", QVariant::fromValue(guidesModel.data()) );

	QString qmlpath = KStandardDirs::locate("appdata","skyguides/qml/GuidesListView.qml");
	mBaseView.setSource(qmlpath);
	mBaseView.setResizeMode(QDeclarativeView::SizeViewToRootObject);
}
