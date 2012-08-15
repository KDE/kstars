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


#include "QGraphicsObject"
#include "wiview.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"

#include "kstandarddirs.h"

WIView::WIView(QWidget *parent, ObsConditions *obs) : QWidget(parent)
{

    m = new ModelManager(obs);

    KStars *kstars = KStars::Instance();

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

    m_SlewButtonObj = m_BaseObj->findChild<QObject *>("slewButtonObj");
    connect(m_SlewButtonObj, SIGNAL(slewButtonClicked()), this, SLOT(onSlewButtonClicked()));

    m_DetailsButtonObj = m_BaseObj->findChild<QObject *>("detailsButtonObj");
    connect(m_DetailsButtonObj, SIGNAL(detailsButtonClicked()), this, SLOT(onDetailsButtonClicked()));

    baseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    baseView->show();
    kstars->setWIView(baseView);
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
//    QObject *sbTextObj = m_DetailsViewObj->findChild<QObject *>("sbTextObj");
    QObject *sizeTextObj = m_DetailsViewObj->findChild<QObject *>("sizeTextObj");

    sonameObj->setProperty("text", soitem->getLongName());
    posTextObj->setProperty("text", soitem->getPosition());
    descTextObj->setProperty("text", soitem->getDesc());

    QString magText = QString("Magnitude: ") + QString::number(soitem->getMagnitude());
    magTextObj->setProperty("text", magText);

    QString sizeText = QString("Size: ") + soitem->getSize();
    sizeTextObj->setProperty("text", sizeText);
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

void WIView::onSlewButtonClicked()
{
    ///Slew map to selected sky-object
    SkyObject* so = m_CurSoItem->getSkyObject();
    KStars* kstars = KStars::Instance();
    if (so != 0)
    {
        kstars->map()->setFocusPoint(so);
        kstars->map()->setFocusObject(so);
        kstars->map()->setDestination(*kstars->map()->focusPoint());
    }
}

void WIView::onDetailsButtonClicked()
{
    ///Code taken from WUTDialog::slotDetails()
    KStars *kstars = KStars::Instance();
    SkyObject* so = m_CurSoItem->getSkyObject();
    DetailDialog *detail = new DetailDialog(so, kstars->data()->lt(), kstars->data()->geo(), kstars);
    detail->exec();
    delete detail;
}
