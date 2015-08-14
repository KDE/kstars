/***************************************************************************
                          skyguideview.h  -  K Desktop Planetarium
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

#ifndef SKYGUIDEVIEW_H
#define SKYGUIDEVIEW_H

#include <QDockWidget>
#include <QQuickView>

#include "skyguideobject.h"

/**
 * @class SkyGuideView
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */
class SkyGuideView : public QQuickView
{

public:
    SkyGuideView();

    QDockWidget* dock() { return m_dock; }

    void setVisible(bool visible);
    void setModel(QList<QObject*> guides);

private:
    QDockWidget* m_dock;
    bool m_firstLoading;

    void initDock();
};

#endif // SKYGUIDEVIEW_H
