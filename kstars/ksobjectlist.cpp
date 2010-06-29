/***************************************************************************
                          ksobjectlist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed June 8 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksobjectlist.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "kdebug.h"
#include "tools/altvstime.h"

#ifdef HAVE_INDI_H
#include "indi/indimenu.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/indidevice.h"
#include "indi/devicemanager.h"
#include "indi/indistd.h"
#endif

KSObjectList::KSObjectList(QWidget *parent):QTableView(parent)
{
    m_SkyObjectList = NULL;
    ks = KStars::Instance();
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(slotContextMenu(const QPoint &)));

    pmenu = new ObjListPopupMenu(this);

    m_showAVT = m_showAddToSession = m_showCenter = m_showDetails = m_showScope = m_showLinks = false;
    m_showAddVisibleTonight = m_showRemoveFromWishList = m_showRemoveFromSessionPlan = false;
}

KSObjectList::~KSObjectList()
{
    delete pmenu;
}

// Read functions for Q_PROPERTY items
bool KSObjectList::showAddToSession() const { return m_showAddToSession; }
bool KSObjectList::showAddVisibleTonight() const { return m_showAddVisibleTonight; }
bool KSObjectList::showCenter() const { return m_showCenter; }
bool KSObjectList::showAVT() const { return m_showAVT; }
bool KSObjectList::showDetails() const { return m_showDetails; }
bool KSObjectList::showScope() const { return m_showScope; }
bool KSObjectList::showLinks() const { return m_showLinks; }
bool KSObjectList::showRemoveFromWishList() const { return m_showRemoveFromWishList; }
bool KSObjectList::showRemoveFromSessionPlan() const { return m_showRemoveFromSessionPlan; }

// Write functions for Q_PROPERTY items
void KSObjectList::setShowAddToSession(const bool &visible) { m_showAddToSession = visible; }
void KSObjectList::setShowAddVisibleTonight(const bool &visible) { m_showAddVisibleTonight = visible; }
void KSObjectList::setShowCenter(const bool &visible) { m_showCenter = visible; }
void KSObjectList::setShowAVT(const bool &visible) { m_showAVT = visible; }
void KSObjectList::setShowDetails(const bool &visible) { m_showDetails = visible; }
void KSObjectList::setShowScope(const bool &visible) { m_showScope = visible; }
void KSObjectList::setShowLinks(const bool &visible) { m_showLinks = visible; }
void KSObjectList::setShowRemoveFromWishList(const bool &visible) { m_showRemoveFromWishList = visible; }
void KSObjectList::setShowRemoveFromSessionPlan(const bool &visible) { m_showRemoveFromSessionPlan = visible; }

// Public slots
void KSObjectList::slotCenterObject()
{
    if (m_SkyObjectList->size() == 1) {
        kDebug() << m_SkyObjectList->first()->name();
        
        ks->map()->setClickedObject( m_SkyObjectList->first() );
        ks->map()->setClickedPoint( m_SkyObjectList->first() );
        ks->map()->slotCenter();
    }
}

void KSObjectList::slotAddToSession()
{
    foreach (SkyObject *o, *m_SkyObjectList) {
        ks->observingList()->slotAddObject(o, true, false);
    }
}

void KSObjectList::slotDetails() {
    if ( m_SkyObjectList->size() == 1 ) {
        QPointer<DetailDialog> dd = new DetailDialog( m_SkyObjectList->first(), ks->data()->lt(), ks->data()->geo(), ks );
        dd->exec();
        delete dd;
    }
}

void KSObjectList::slotAVT() {
    QPointer<AltVsTime> avt = new AltVsTime( KStars::Instance() );//FIXME KStars class is singleton, so why pass it?
    kDebug() << "Getting here";
    foreach (SkyObject *o, *m_SkyObjectList) {
        avt->processObject(o);
    }
    avt->exec();
    delete avt;
}

void KSObjectList::slotSlewToObject()
{
#ifdef HAVE_INDI_H
    INDI_D *indidev(NULL);
    INDI_P *prop(NULL), *onset(NULL);
    INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *ConnectEle(NULL), *nameEle(NULL);
    bool useJ2000( false);
    int selectedCoord(0);
    SkyPoint sp;

    // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
    // i.e. the first telescope we find!
    INDIMenu *imenu = ks->indiMenu();

    for (int i=0; i < imenu->managers.size() ; i++)
    {
        for (int j=0; j < imenu->managers.at(i)->indi_dev.size(); j++)
        {
            indidev = imenu->managers.at(i)->indi_dev.at(j);
            indidev->stdDev->currentObject = NULL;
            prop = indidev->findProp("EQUATORIAL_EOD_COORD");
            if (prop == NULL)
            {
                prop = indidev->findProp("EQUATORIAL_COORD");
                if (prop == NULL)
                {
                    prop = indidev->findProp("HORIZONTAL_COORD");
                    if (prop == NULL)
                        continue;
                    else
                        selectedCoord = 1;      /* Select horizontal */
                }
                else
                    useJ2000 = true;
            }

            ConnectEle = indidev->findElem("CONNECT");
            if (!ConnectEle) continue;

            if (ConnectEle->state == PS_OFF)
            {
                KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", indidev->label));
                return;
            }

            switch (selectedCoord)
            {
                // Equatorial
            case 0:
                if (prop->perm == PP_RO) continue;
                RAEle  = prop->findElement("RA");
                if (!RAEle) continue;
                DecEle = prop->findElement("DEC");
                if (!DecEle) continue;
                break;

                // Horizontal
            case 1:
                if (prop->perm == PP_RO) continue;
                AzEle = prop->findElement("AZ");
                if (!AzEle) continue;
                AltEle = prop->findElement("ALT");
                if (!AltEle) continue;
                break;
            }

            onset = indidev->findProp("ON_COORD_SET");
            if (!onset) continue;

            onset->activateSwitch("SLEW");

            indidev->stdDev->currentObject = m_SkyObjectList->first();

            /* Send object name if available */
            if (indidev->stdDev->currentObject)
            {
                nameEle = indidev->findElem("OBJECT_NAME");
                if (nameEle && nameEle->pp->perm != PP_RO)
                {
                    nameEle->write_w->setText(indidev->stdDev->currentObject->name());
                    nameEle->pp->newText();
                }
            }

            switch (selectedCoord)
            {
            case 0:
                if (indidev->stdDev->currentObject)
                    sp = *indidev->stdDev->currentObject;
                else
                    sp = *ks->map()->clickedPoint();

                if (useJ2000)
                    sp.apparentCoord(ks->data()->ut().djd(), (long double) J2000);

                RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra().hour()).arg(sp.ra().minute()).arg(sp.ra().second()));
                DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec().degree()).arg(sp.dec().arcmin()).arg(sp.dec().arcsec()));

                break;

            case 1:
                if (indidev->stdDev->currentObject)
                {
                    sp.setAz( indidev->stdDev->currentObject->az());
                    sp.setAlt(indidev->stdDev->currentObject->alt());
                }
                else
                {
                    sp.setAz( ks->map()->clickedPoint()->az());
                    sp.setAlt(ks->map()->clickedPoint()->alt());
                }

                AzEle->write_w->setText(QString("%1:%2:%3").arg(sp.az().degree()).arg(sp.az().arcmin()).arg(sp.az().arcsec()));
                AltEle->write_w->setText(QString("%1:%2:%3").arg(sp.alt().degree()).arg(sp.alt().arcmin()).arg(sp.alt().arcsec()));

                break;
            }

            prop->newText();

            return;
        }
    }

    // We didn't find any telescopes
    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));

#endif
}
void KSObjectList::slotContextMenu(const QPoint &pos)
{
    int countRows = selectionModel()->selectedRows().count();
    QPoint localPos = QWidget::mapToGlobal(pos);

    if (countRows >= 2) {
        // Items that can be displayed when multiple rows are selected
        pmenu->init();

        if (m_showAddToSession) {
            pmenu->showAddToSession();
        }
        if (m_showAddVisibleTonight) {
            pmenu->showAddVisibleTonight();
        }
        pmenu->addSeparator();

        if (m_showAVT) {
            pmenu->showAVT();
        }
        pmenu->addSeparator();

        if (m_showRemoveFromWishList) {
            pmenu->showRemoveFromWishList();
        }

        if (m_showRemoveFromSessionPlan) {
            pmenu->showRemoveFromSessionPlan();
        }

        pmenu->popup(localPos);
    } else if (countRows == 1) {
        // Items displayed when only one row is selected
        pmenu->init();

        if (m_showAddToSession) {
            pmenu->showAddToSession();
        }
        if (m_showAddVisibleTonight) {
            pmenu->showAddVisibleTonight();
        }
        pmenu->addSeparator();

        if (m_showCenter) {
            pmenu->showCenter();
        }
#ifdef HAVE_INDI_H
        if (m_showScope) {
            pmenu->showScope();
        }
#endif
        pmenu->addSeparator();

        if (m_showDetails) {
            pmenu->showDetails();
        }
        if (m_showAVT) {
            pmenu->showAVT();
        }
        pmenu->addSeparator();

        if (m_showLinks) {
            pmenu->showLinks();
        }
        pmenu->addSeparator();

        if (m_showRemoveFromWishList) {
            pmenu->showRemoveFromWishList();
        }
        if (m_showRemoveFromSessionPlan) {
            pmenu->showRemoveFromSessionPlan();
        }

        pmenu->popup(localPos);
    }
}

void KSObjectList::setSkyObjectList (QList<SkyObject *> l)
{
    if (m_SkyObjectList) {
        delete m_SkyObjectList;
    }
    m_SkyObjectList = new QList<SkyObject *>(l);
}

QList<SkyObject *> * KSObjectList::getSkyObjectList()
{
    return m_SkyObjectList;
}


