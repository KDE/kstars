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

#include "WIView.h"
#include "QGraphicsObject"
#include "skymap.h"

#include "kstandarddirs.h"

WIView::WIView ( QObject *parent, ObsConditions *obs) : QObject(parent)
{

    m = new ModelManager(obs);

    QDeclarativeView *baseView = new QDeclarativeView();
    ctxt = baseView->rootContext();
    ctxt->setContextProperty("catListModel", QVariant::fromValue(m->returnCatListModel( ModelManager::BaseList )));

    baseView->setSource(KStandardDirs::locate("appdata","tools/WhatsInteresting/Base.qml") );
    baseObj = dynamic_cast<QObject *> (baseView->rootObject());

    soTypeTextObj = baseObj->findChild<QObject *>("soTypeTextObj") ;

    catListObj = baseObj->findChild<QObject *>("catListObj");
    connect(catListObj, SIGNAL(catListItemClicked(QString)), this, SLOT(onCatListItemClicked(QString)));

    soListObj = baseObj->findChild<QObject *>("soListObj");
    connect(soListObj, SIGNAL(soListItemClicked(int, QString, int)), this, SLOT(onSoListItemClicked(int, QString, int)));

    detailsViewObj = baseObj->findChild<QObject *>("detailsViewObj");
    nextObj  = baseObj->findChild<QObject *>("nextObj");
    connect(nextObj, SIGNAL(nextObjTextClicked()), this, SLOT(onNextObjTextClicked()));

    baseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    baseView->show();
}

WIView::~WIView()
{
    delete m;
    delete curSoItem;
}

void WIView::onCatListItemClicked(QString category)
{
    if (category == "Deep-sky Objects")
    {
        ctxt->setContextProperty("catListModel", QVariant::fromValue(m->returnCatListModel( ModelManager::DeepSkyObjects )));
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
    }
    else if (category == "Planets")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Planets ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
    }
    else if (category == "Stars")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Stars ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
    }
    else if (category == "Galaxies")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Galaxies ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
    }
    else if (category == "Constellations")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Constellations ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
    }
    else if (category == "Star Clusters")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Star_Clusters ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
    }
    else if (category == "Nebulae")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Nebulae ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
        soTypeTextObj->setProperty("text", category);
        soTypeTextObj->setProperty("visible", true);
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
    default:
        kDebug()<<"Nothing for DSOs right now";
        return;
    }

//    if (type == "Star")
//        soitem = m->returnModel(ModelManager::Stars)->getSkyObjItem(index);
//    else if (type == "Constellation")
//        soitem = m->returnModel(ModelManager::Constellations)->getSkyObjItem(index);
//    else if (type == "Planet")
//        soitem = m->returnModel(ModelManager::Planets)->getSkyObjItem(index);
//    else
//    {
//        kDebug()<<"Nothing for DSOs right now";
//        return;
//    }

    kDebug()<<soitem->getName()<<soitem->getType();
    soTypeTextObj->setProperty("text", typeName);
    soTypeTextObj->setProperty("visible", true);

    soListObj->setProperty("visible", false);
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

    detailsViewObj->setProperty("visible", true);

    //Slew map to selected sky object
    SkyObject* so = soitem->getSkyObject();
    KStars* data = KStars::Instance();
    if (so != 0) {
        data->map()->setFocusPoint( so );
        data->map()->setFocusObject( so );
        data->map()->setDestination( *data->map()->focusPoint() );
    }

    curSoItem = soitem;
    curIndex = index;
}

void WIView::onNextObjTextClicked()
{
    int modelSize = m->returnModel(curSoItem->getTypeName())->rowCount();
    SkyObjItem *nextItem = m->returnModel(curSoItem->getTypeName())->getSkyObjItem((curIndex+1)%modelSize);
    loadDetailsView(nextItem, (curIndex+1)%modelSize);
}
