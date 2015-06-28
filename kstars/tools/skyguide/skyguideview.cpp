/***************************************************************************
                          skyguideview.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/06/27
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

#include "skyguideview.h"

SkyGuideView::SkyGuideView()
{
    QString qmlViewPath = "tools/skyguide/resources/skyguideview.qml";
    this->setSource(QStandardPaths::locate(QStandardPaths::DataLocation, qmlViewPath));

    QWidget* container = QWidget::createWindowContainer(this);
    container->setMinimumWidth(this->width());
    container->setMaximumWidth(this->width());
    container->setFocusPolicy(Qt::TabFocus);

    m_dock = new QDockWidget();
    m_dock->setObjectName("Sky Guide");
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dock->setWidget(container);
    m_dock->setMinimumWidth(this->width());

    this->show();
}
