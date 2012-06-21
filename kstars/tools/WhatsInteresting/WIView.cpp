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

#include "QDeclarativeContext"
#include "WIView.h"
#include "QGraphicsObject"

WIView::WIView ( QObject *parent) : QObject(parent)
{

    m = new ModelManager();

    QDeclarativeView *baseView;
    baseView = new QDeclarativeView();
    ctxt = baseView->rootContext();
    ctxt->setContextProperty("catListModel", QVariant::fromValue(m->returnCatListModel( ModelManager::BaseList )));
    //baseListBox = new QDeclarativeView();
    //kDebug()<<QUrl::fromLocalFile("Base.qml");
    baseView->setSource(QUrl("/home/sam/kstars/kstars/tools/WhatsInteresting/Base.qml"));
    baseObj = dynamic_cast<QObject *> (baseView->rootObject());

    catListObj = baseObj->findChild<QObject *>("listContainer")->findChild<QObject *>("catListObj");
    connect(catListObj, SIGNAL(catListItemClicked(QString)), this, SLOT(onCatListItemClicked(QString)));
    soListObj = baseObj->findChild<QObject *>("listContainer")->findChild<QObject *>("soListObj");
    connect(soListObj, SIGNAL(soListItemClicked(QString, QString)),
            this, SLOT(onSoListItemClicked(QString, QString)));


//     planetaryListView->setSource(QUrl::fromLocalFile("WIPlanetaryListView.qml"));
//     QObject *planetaryListObj = planetaryListView->rootObject();
//     deepSkyListView->setSource(QUrl::fromLocalFile("WIDeepSkyListView.qml"));
//     QObject *deepSkyListObj = deepSkyListView->rootObject();


//     skyObjListView = new QDeclarativeView();
//     skyObjListView->rootContext()->setContextProperty("skyObjModel", m->returnModel(ModelManager::Planets));
//     skyObjListView->setSource(QUrl("/home/sam/kstars/kstars/tools/WhatsInteresting/SkyObjListView.qml"));
// 
//     kDebug()<<skyObjListView->rootContext()->contextProperty("skyObjModel");
    //skyObjListView->setParent(baseListBox);
    //QObject *skyObjectListObj = dynamic_cast<QObject *> (skyObjListView->rootObject());
    //skyObjectListObj->setParent(listBoxObj);
    baseView->show();
    //skyObjectListObj->setParent(baseObj);
    //skyObjectListObj->setProperty("skyObjModel", m->returnModel(2));
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

void WIView::onSoListItemClicked(QString name, QString type)
{
    kDebug()<<"Working"<<name<<type;
}

