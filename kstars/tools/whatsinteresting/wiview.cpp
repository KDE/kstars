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
#include "kdeclarative.h"

WIView::WIView(QWidget *parent, ObsConditions *obs) : QWidget(parent), m_Obs(obs)
{
    m_ModManager = new ModelManager(obs);

    m_BaseView = new QDeclarativeView();

    ctxt = m_BaseView->rootContext();

    m_BaseView->setSource(KStandardDirs::locate("appdata","tools/whatsinteresting/qml/wiview.qml"));

    m_BaseObj = dynamic_cast<QObject *>(m_BaseView->rootObject());

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

    QObject *settingsIconObj = m_BaseObj->findChild<QObject *>("settingsIconObj");
    connect(settingsIconObj, SIGNAL(settingsIconClicked()), this, SLOT(onSettingsIconClicked()));

    m_BaseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_BaseView->show();
}

WIView::~WIView()
{
    delete m_ModManager;
    delete m_CurSoItem;
}

void WIView::onCategorySelected(int type)
{
    ctxt->setContextProperty("soListModel", m_ModManager->returnModel(type));
}

void WIView::onSoListItemClicked(int type, QString typeName, int index)
{
    SkyObjItem *soitem = m_ModManager->returnModel(type)->getSkyObjItem(index);

//    soTypeTextObj->setProperty("text", typeName);
//    soTypeTextObj->setProperty("visible", true);

//    soListObj->setProperty("visible", false);

    loadDetailsView(soitem, index);
}

void WIView::loadDetailsView(SkyObjItem *soitem, int index)
{

    m_CurSoItem = soitem;
    m_CurIndex = index;

    int modelSize = m_ModManager->returnModel(m_CurSoItem->getType())->rowCount();
    SkyObjItem *nextItem = m_ModManager->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex+1)%modelSize);
    SkyObjItem *prevItem = m_ModManager->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);

    QObject *nextTextObj = m_NextObj->findChild<QObject *>("nextTextObj");
    nextTextObj->setProperty("text", nextItem->getName());
    QObject *prevTextObj = m_PrevObj->findChild<QObject *>("prevTextObj");
    prevTextObj->setProperty("text", prevItem->getName());

    QObject *sonameObj = m_DetailsViewObj->findChild<QObject *>("sonameObj");
    QObject *posTextObj = m_DetailsViewObj->findChild<QObject *>("posTextObj");
    QObject *descTextObj = m_DetailsViewObj->findChild<QObject *>("descTextObj");
    QObject *magTextObj = m_DetailsViewObj->findChild<QObject *>("magTextObj");
    QObject *sbTextObj = m_DetailsViewObj->findChild<QObject *>("sbTextObj");
    QObject *sizeTextObj = m_DetailsViewObj->findChild<QObject *>("sizeTextObj");

    sonameObj->setProperty("text", soitem->getLongName());
    posTextObj->setProperty("text", soitem->getPosition());
    descTextObj->setProperty("text", soitem->getDesc());

    QString magText;
    if (soitem->getType() == SkyObjItem::Constellation)
        magText = i18n("Magnitude:  --");
    else
        magText = i18n("Magnitude: ") + KGlobal::locale()->formatNumber(soitem->getMagnitude(), 2) + " mag";
    magTextObj->setProperty("text", magText);

    QString sbText = i18n("Surface Brightness") + ": " + soitem->getSurfaceBrightness();
    sbTextObj->setProperty("text", sbText);

    QString sizeText = i18n("Size") + ": " + soitem->getSize();
    sizeTextObj->setProperty("text", sizeText);
}

void WIView::onNextObjClicked()
{
    int modelSize = m_ModManager->returnModel(m_CurSoItem->getType())->rowCount();
    SkyObjItem *nextItem = m_ModManager->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex+1)%modelSize);
    loadDetailsView(nextItem, (m_CurIndex+1)%modelSize);
}

void WIView::onPrevObjClicked()
{
    int modelSize = m_ModManager->returnModel(m_CurSoItem->getType())->rowCount();
    SkyObjItem *prevItem = m_ModManager->returnModel(m_CurSoItem->getType())->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);
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

void WIView::onSettingsIconClicked()
{
    KStars *kstars = KStars::Instance();
    kstars->showWIWizard();
}
