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

WIView::WIView ( QObject *parent) : QObject(parent)
{

    m = new ModelManager();

    QDeclarativeView *baseView;
    baseView = new QDeclarativeView();
    ctxt = baseView->rootContext();
    ctxt->setContextProperty("catListModel", QVariant::fromValue(m->returnCatListModel( ModelManager::BaseList )));

    baseView->setSource(QUrl("/home/sam/kstars/kstars/tools/WhatsInteresting/Base.qml"));
    baseObj = dynamic_cast<QObject *> (baseView->rootObject());

    catListObj = baseObj->findChild<QObject *>("container")->findChild<QObject *>("catListObj");
    connect(catListObj, SIGNAL(catListItemClicked(QString)), this, SLOT(onCatListItemClicked(QString)));
    soListObj = baseObj->findChild<QObject *>("container")->findChild<QObject *>("soListObj");
    connect(soListObj, SIGNAL(soListItemClicked(QString, int)),
            this, SLOT(onSoListItemClicked(QString, int)));
    detailsViewObj = baseObj->findChild<QObject *>("container")->findChild<QObject *>("detailsViewObj");
    nextObj  = baseObj->findChild<QObject *>("nextObj");
    connect(nextObj, SIGNAL(nextObjTextClicked()), this, SLOT(onNextObjTextClicked()));

    baseView->show();
}

WIView::~WIView() {}

void WIView::onCatListItemClicked(QString category)
{
    if (category == "Planetary Objects")
    {
        kDebug()<<"Planetary Objects";
        ctxt->setContextProperty("catListModel", QVariant::fromValue(m->returnCatListModel( ModelManager::PlanetaryObjects )));
    }
    else if (category == "Deep-sky Objects")
    {
        ctxt->setContextProperty("catListModel", QVariant::fromValue(m->returnCatListModel( ModelManager::DeepSkyObjects )));
    }
    else if (category == "Planets")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Planets ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
    }
    else if (category == "Stars")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Stars ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
    }
    else if (category == "Galaxies")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Galaxies ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
    }
    else if (category == "Constellations")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Constellations ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
    }
    else if (category == "Star Clusters")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Star_Clusters ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
    }
    else if (category == "Nebulae")
    {
        ctxt->setContextProperty("soListModel", m->returnModel( ModelManager::Nebulae ));
        catListObj->setProperty("visible", false);
        soListObj->setProperty("visible", true);
    }
}

void WIView::onSoListItemClicked(QString type, int index)
{
    SkyObjItem *soitem;
    if (type == "Star")
        soitem = m->returnModel(ModelManager::Stars)->getSkyObjItem(index);
    else if (type == "Constellation")
        soitem = m->returnModel(ModelManager::Constellations)->getSkyObjItem(index);
    else if (type == "Planet")
        soitem = m->returnModel(ModelManager::Planets)->getSkyObjItem(index);
    kDebug()<<soitem->getName()<<soitem->getType();

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
    int modelSize = m->returnModel(curSoItem->getType())->rowCount();
    SkyObjItem *nextItem = m->returnModel(curSoItem->getType())->getSkyObjItem((curIndex+1)%modelSize);
    loadDetailsView(nextItem, (curIndex+1)%modelSize);
}