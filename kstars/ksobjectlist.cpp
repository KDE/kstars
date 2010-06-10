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

#include <QDebug>
#include <QObject>
#include <QItemSelectionModel>
#include "ksobjectlist.h"

KSObjectList::KSObjectList(QWidget *parent):QTableView(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(slotContextMenu(const QPoint &)));
    pmenu = new ObjListPopupMenu();

    m_showAVT = true;
    m_showAddToSession = m_showCenter = m_showDetails = m_showScope = m_showLinks = false;
    m_showAddVisibleTonight = m_showRemoveFromWishList = m_showRemoveFromSessionPlan = false;
}

KSObjectList::~KSObjectList()
{
//  delete pmenu;
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
void KSObjectList::slotContextMenu(const QPoint &pos)
{
    int countRows = selectionModel()->selectedRows().count();
    QPoint localPos = QWidget::mapToGlobal(pos);

    if (countRows >= 2) {
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

    if (countRows == 1) {
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
        if (m_showScope) {
            pmenu->showScope();
        }
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

