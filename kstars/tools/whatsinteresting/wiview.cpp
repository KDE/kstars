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

WIView::WIView(QWidget *parent, ObsConditions *obs) : QWidget(parent)
{

    m = new ModelManager(obs);

    KStars *data = KStars::Instance();

    QDeclarativeView *baseView = new QDeclarativeView();

    ctxt = baseView->rootContext();

    baseView->setSource(KStandardDirs::locate("appdata","tools/whatsinteresting/qml/wiview.qml"));

    m_BaseObj = dynamic_cast<QObject *>(baseView->rootObject());

    //soTypeTextObj = m_BaseObj->findChild<QObject *>("soTypeTextObj");

    m_ViewsRowObj = m_BaseObj->findChild<QObject *>("viewsRowObj");
    connect(m_ViewsRowObj, SIGNAL(categorySelected(int)), this, SLOT(onCategorySelected(int)));

    m_SoListObj = m_BaseObj->findChild<QObject *>("soListObj");
    connect(m_SoListObj, SIGNAL(soListItemClicked(int, QString, int)), this, SLOT(onSoListItemClicked(int, QString, int)));

    m_DetailsViewObj = m_BaseObj->findChild<QObject *>("detailsViewObj");

    m_NextObj = m_BaseObj->findChild<QObject *>("nextObj");
    connect(m_NextObj, SIGNAL(nextObjClicked()), this, SLOT(onNextObjClicked()));
    m_PrevObj = m_BaseObj->findChild<QObject *>("prevObj");
    connect(m_PrevObj, SIGNAL(prevObjClicked()), this, SLOT(onPrevObjClicked()));

    QObject *closeButtonObj = m_BaseObj->findChild<QObject *>("closeButtonObj");
    connect(closeButtonObj, SIGNAL(closeButtonClicked()), baseView, SLOT(close()));

    baseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    baseView->show();
    data->setWIView(baseView);
}

WIView::~WIView()
{
    delete m;
    delete m_CurSoItem;
}

void WIView::onCategorySelected(int type)
{
    switch(type)
    {
    case 0:                        ///Planet type
    case 1:                        ///Star type
    case 2:                        ///Constellation type
        ctxt->setContextProperty("soListModel", m->returnModel(type));
        break;
    case 3:                        ///Galaxy type
    case 4:                        ///Cluster type
    case 5:                        ///Nebula type
        ctxt->setContextProperty("soListModel", m->returnModel(type));
        break;
    }
}

void WIView::onSoListItemClicked(int type, QString typeName, int index)
{
    SkyObjItem *soitem = m->returnModel(type)->getSkyObjItem(index);

//    soTypeTextObj->setProperty("text", typeName);
//    soTypeTextObj->setProperty("visible", true);

//    soListObj->setProperty("visible", false);

    loadDetailsView(soitem, index);
}

void WIView::loadDetailsView(SkyObjItem *soitem, int index)
{

    m_CurSoItem = soitem;
    m_CurIndex = index;

    int modelSize = m->returnModel(m_CurSoItem->getType())->rowCount();
    SkyObjItem *nextItem = m->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex+1)%modelSize);
    SkyObjItem *prevItem = m->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);
    //QString nextObjText = QString("Next: ") + nextItem->getName();
    QObject *nextTextObj = m_NextObj->findChild<QObject *>("nextTextObj");
    nextTextObj->setProperty("text", nextItem->getName());
    //QString prevObjText = QString("Previous: ") + prevItem->getName();
    QObject *prevTextObj = m_PrevObj->findChild<QObject *>("prevTextObj");
    prevTextObj->setProperty("text", prevItem->getName());

    QObject *sonameObj = m_DetailsViewObj->findChild<QObject *>("sonameObj");
    QObject *posTextObj = m_DetailsViewObj->findChild<QObject *>("posTextObj");
    QObject *descTextObj = m_DetailsViewObj->findChild<QObject *>("descTextObj");
    QObject *magTextObj = m_DetailsViewObj->findChild<QObject *>("magTextObj");
    sonameObj->setProperty("text", soitem->getLongName());
    posTextObj->setProperty("text", soitem->getPosition());
    descTextObj->setProperty("text", soitem->getDesc());
    magTextObj->setProperty("text", soitem->getMagnitude());

    ///Slew map to selected sky-object
    SkyObject* so = soitem->getSkyObject();
    KStars* data = KStars::Instance();
    if (so != 0)
    {
        data->map()->setFocusPoint(so);
        data->map()->setFocusObject(so);
        data->map()->setDestination(*data->map()->focusPoint());
    }
}

void WIView::onNextObjClicked()
{
    int modelSize = m->returnModel(m_CurSoItem->getType())->rowCount();
    SkyObjItem *nextItem = m->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex+1)%modelSize);
    loadDetailsView(nextItem, (m_CurIndex+1)%modelSize);
}

void WIView::onPrevObjClicked()
{
    int modelSize = m->returnModel(m_CurSoItem->getType())->rowCount();
    SkyObjItem *prevItem = m->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);
    loadDetailsView(prevItem, (m_CurIndex-1+modelSize)%modelSize);
}
