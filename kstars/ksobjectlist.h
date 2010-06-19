/***************************************************************************
                          ksobjectlist.h  -  K Desktop Planetarium
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

#ifndef KSOBJECTLIST_H_
#define KSOBJECTLIST_H_

#include <QTableView>
#include <QWidget>
#include <QPoint>
#include <QEvent>
#include <QMouseEvent>

#include "objlistpopupmenu.h"

class KSObjectList : public QTableView
{
    Q_OBJECT

    Q_ENUMS ( showAddToSession );
    Q_PROPERTY ( bool showAddToSession READ showAddToSession WRITE setShowAddToSession )
    Q_PROPERTY ( bool showAddVisibleTonight READ showAddVisibleTonight WRITE setShowAddVisibleTonight )
    Q_PROPERTY ( bool showCenter READ showCenter WRITE setShowCenter )
    Q_PROPERTY ( bool showDetails READ showDetails WRITE setShowDetails )
    Q_PROPERTY ( bool showAVT READ showAVT WRITE setShowAVT )
    Q_PROPERTY ( bool showScope READ showScope WRITE setShowScope )
    Q_PROPERTY ( bool showLinks READ showLinks WRITE setShowLinks )
    Q_PROPERTY ( bool showRemoveFromWishList READ showRemoveFromWishList WRITE setShowRemoveFromWishList )
    Q_PROPERTY ( bool showRemoveFromSessionPlan READ showRemoveFromSessionPlan WRITE setShowRemoveFromSessionPlan )

public:
    KSObjectList(QWidget *parent);
    ~KSObjectList();

    /* Read functions for properties */
    bool showAddToSession() const;
    bool showAddVisibleTonight() const;
    bool showCenter() const;
    bool showAVT() const;
    bool showDetails() const;
    bool showScope() const;
    bool showLinks() const;
    bool showRemoveFromWishList() const;
    bool showRemoveFromSessionPlan() const;

    /* Write functions for properties */
    void setShowAddToSession(const bool &);
    void setShowAddVisibleTonight(const bool &);
    void setShowCenter(const bool &);
    void setShowAVT(const bool &);
    void setShowDetails(const bool &);
    void setShowScope(const bool &);
    void setShowLinks(const bool &);
    void setShowRemoveFromWishList(const bool &);
    void setShowRemoveFromSessionPlan(const bool &);

public slots:
    void slotContextMenu(const QPoint &pos);

private:
    ObjListPopupMenu *pmenu;
    bool singleSelection, noSelection;
    bool m_showAddToSession, m_showCenter, m_showDetails, m_showScope, m_showLinks;
    bool m_showAddVisibleTonight, m_showAVT, m_showRemoveFromWishList, m_showRemoveFromSessionPlan;
};

#endif
