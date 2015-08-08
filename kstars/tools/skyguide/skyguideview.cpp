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

#include <QQmlContext>
#include <QStandardPaths>

#include "kstars.h"
#include "skyguideview.h"

SkyGuideView::SkyGuideView() : m_firstLoading(true) {
    m_dock = new QDockWidget("SkyGuide");
    m_dock->setObjectName("SkyGuide");
}

void SkyGuideView::setModel(QList<QObject*> guides) {
    this->setResizeMode(QQuickView::SizeRootObjectToView);
    QQmlContext* ctxt = this->rootContext();
    ctxt->setContextProperty("guidesModel", QVariant::fromValue(guides));

    if (this->source().isEmpty()) {
        QString qmlViewPath = "tools/skyguide/resources/SkyGuideView.qml";
        this->setSource(QStandardPaths::locate(QStandardPaths::DataLocation, qmlViewPath));
        initDock();
    }
}

void SkyGuideView::initDock() {
    QWidget* container = QWidget::createWindowContainer(this);
    container->setMinimumWidth(this->width());
    container->setMaximumWidth(this->width());
    container->setFocusPolicy(Qt::TabFocus);
    m_dock->setWidget(container);
    m_dock->setMinimumWidth(this->width());
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

void SkyGuideView::setVisible(bool visible) {
    if (visible) {
        this->show();
    }
    m_dock->setVisible(visible);
    if (m_firstLoading) { // adds the dockwidget only once
        KStars::Instance()->addDockWidget(Qt::RightDockWidgetArea, m_dock);
        m_firstLoading = false;
    }
}
