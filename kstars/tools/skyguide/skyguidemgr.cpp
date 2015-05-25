/***************************************************************************
                          skyguidemgr.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/05/06
    copyright            : (C) 2015 by Marcos Cardinot
    email                : mcardinot@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QStandardPaths>

#include "skyguidemgr.h"

SkyGuideMgr::SkyGuideMgr()
{
    m_view = new QQuickView();
    QString qmlViewPath = "tools/skyguide/qml/skyguideview.qml";
    m_view->setSource(QStandardPaths::locate(QStandardPaths::DataLocation, qmlViewPath));

    m_container = QWidget::createWindowContainer(m_view);
    m_container->setMinimumSize(400, 400);
    m_container->setMaximumSize(400, 400);
    m_container->setFocusPolicy(Qt::TabFocus);
}

SkyGuideMgr::~SkyGuideMgr()
{
}
