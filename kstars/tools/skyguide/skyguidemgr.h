/***************************************************************************
                          skyguidemgr.h  -  K Desktop Planetarium
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

#ifndef SKYGUIDEMGR_H
#define SKYGUIDEMGR_H

#include "skyguideobject.h"
#include "skyguideview.h"

class SkyGuideMgr : public QObject
{
    Q_OBJECT
public:
    SkyGuideMgr();
    virtual ~SkyGuideMgr();

    inline SkyGuideView* view() { return m_view; }

private slots:
    void slotAddSkyGuide();

private:
    SkyGuideView* m_view;
    QList<QObject*> m_skyGuideObjects;
    QDir m_guidesDir;

    void loadAllSkyGuideObjects();
    void loadSkyGuideObject(const QString& guidePath, const QString &filename);
};

#endif // SKYGUIDEMGR_H
