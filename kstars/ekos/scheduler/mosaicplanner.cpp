/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QQuickView>
#include <klocalizedcontext.h>
#include <klocalizedstring.h>
#include <QQmlContext>

#include "mosaicplanner.h"
#include "Options.h"
#include "ekos_scheduler_debug.h"

namespace Ekos
{

MosaicPlanner::MosaicPlanner(QWidget *parent) : QWidget(parent)
{

    m_FocalLength = Options::telescopeFocalLength();
    m_CameraSize.setWidth(Options::cameraWidth());
    m_CameraSize.setHeight(Options::cameraHeight());
    m_PixelSize.setWidth(Options::cameraPixelWidth());
    m_PixelSize.setHeight(Options::cameraPixelHeight());

    // QML Stuff
    m_BaseView = new QQuickView();

    m_BaseView->setTitle(i18n("Mosaic Planner"));
#ifdef Q_OS_OSX
    m_BaseView->setFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#else
    m_BaseView->setFlags(Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
#endif

    // Theming?
    m_BaseView->setColor(Qt::black);

    m_BaseObj = m_BaseView->rootObject();

    m_Ctxt = m_BaseView->rootContext();

    m_Ctxt->setContextObject(new KLocalizedContext(m_BaseView));

    m_Ctxt->setContextProperty("MosaicPlanner", this);

    m_BaseView->setResizeMode(QQuickView::SizeRootObjectToView);

    m_BaseView->setSource(QUrl("qrc:/qml/mosaic/mosaicwizard.qml"));

    m_BaseView->show();
}

MosaicPlanner::~MosaicPlanner()
{
}

}
