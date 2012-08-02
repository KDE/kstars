/***************************************************************************
                          wiview.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "wiview.h"
#include "QGraphicsObject"
#include "skymap.h"

#include "kstandarddirs.h"

WIView::WIView ( QObject *parent, ObsConditions *obs) : QObject(parent)
{

    m = new ModelManager(obs);

    QDeclarativeView *baseView = new QDeclarativeView();
    ctxt = baseView->rootContext();

    baseView->setSource(KStandardDirs::locate("appdata","tools/whatsinteresting/qml/wiview.qml") );
    baseObj = dynamic_cast<QObject *> (baseView->rootObject());

    //soTypeTextObj = baseObj->findChild<QObject *>("soTypeTextObj");

    containerObj = baseObj->findChild<QObject *>("containerObj");

    viewsRowObj = baseObj->findChild<QObject *>("viewsRowObj");
    connect(viewsRowObj, SIGNAL(categorySelected(int)), this, SLOT(onCategorySelected(int)));

    soListObj = baseObj->findChild<QObject *>("soListObj");
    connect(soListObj, SIGNAL(soListItemClicked(int, QString, int)), this, SLOT(onSoListItemClicked(int, QString, int)));

    detailsViewObj = baseObj->findChild<QObject *>("detailsViewObj");
    nextObj  = baseObj->findChild<QObject *>("nextObj");
    connect(nextObj, SIGNAL(nextObjTextClicked()), this, SLOT(onNextObjTextClicked()));

    m_OptMag = obs->getOptimumMAG();

    baseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    baseView->show();
}

WIView::~WIView()
{
    delete m;
    delete curSoItem;
}

void WIView::onCategorySelected(int type)
{
    QString oMagText = QString("Suggested optimum magnification : ") + QString::number(m_OptMag);
    QObject* oMagTextObj = baseObj->findChild<QObject *>("oMagTextObj");

    switch(type)
    {
    case 0:
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Planets ));
        break;
    case 1:
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Stars ));
        break;
    case 2:
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Constellations ));
        break;
    case 3:
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Galaxies ));
        oMagTextObj->setProperty("text", oMagText);
        break;
    case 4:
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Clusters ));
        oMagTextObj->setProperty("text", oMagText);
        break;
    case 5:
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Nebulae ));
        oMagTextObj->setProperty("text", oMagText);
        break;
    }
}

void WIView::onSoListItemClicked(int type, QString typeName, int index)
{
    SkyObjItem *soitem;

    kDebug()<<type;
    switch (type)
    {
    case 0:
        soitem = m->returnModel(ModelManager::Planets)->getSkyObjItem(index);
        break;
    case 1:
        soitem = m->returnModel(ModelManager::Stars)->getSkyObjItem(index);
        break;
    case 2:
        soitem = m->returnModel(ModelManager::Galaxies)->getSkyObjItem(index);
        break;
    case 3:
        soitem = m->returnModel(ModelManager::Constellations)->getSkyObjItem(index);
        break;
    case 4:
        soitem = m->returnModel(ModelManager::Clusters)->getSkyObjItem(index);
        kDebug()<<soitem->getSkyObject()->typeName();
        return;
    case 5:
        soitem = m->returnModel(ModelManager::Nebulae)->getSkyObjItem(index);
        kDebug()<<soitem->getSkyObject()->typeName();
        return;
    }

    kDebug()<<soitem->getName()<<soitem->getType();
//    soTypeTextObj->setProperty("text", typeName);
//    soTypeTextObj->setProperty("visible", true);

//    soListObj->setProperty("visible", false);
    loadDetailsView(soitem , index);
}

void WIView::loadDetailsView(SkyObjItem* soitem, int index)
{
    QObject* sonameObj = detailsViewObj->findChild<QObject *>("sonameObj");
    QObject* posTextObj = detailsViewObj->findChild<QObject *>("posTextObj");
    QObject* descTextObj = detailsViewObj->findChild<QObject *>("descTextObj");
    QObject* magTextObj = detailsViewObj->findChild<QObject *>("magTextObj");
    sonameObj->setProperty("text", soitem->getName());
    posTextObj->setProperty("text", soitem->getPosition());
    descTextObj->setProperty("text", soitem->getDesc());
    magTextObj->setProperty("text", soitem->getMagnitude());

    //Slew map to selected sky object
    SkyObject* so = soitem->getSkyObject();
    KStars* data = KStars::Instance();
    if (so != 0) {
        data->map()->setFocusPoint( so );
        data->map()->setFocusObject( so );
        data->map()->setDestination( *data->map()->focusPoint() );
    }

    curSoItem = soitem;
    m_CurIndex = index;
}

void WIView::onNextObjTextClicked()
{
    int modelSize = m->returnModel(curSoItem->getTypeName())->rowCount();
    SkyObjItem *nextItem = m->returnModel(curSoItem->getTypeName())->getSkyObjItem((m_CurIndex+1)%modelSize);
    loadDetailsView(nextItem, (m_CurIndex+1)%modelSize);
}
