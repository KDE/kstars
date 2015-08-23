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
#include "skyguidewriter.h"

#define JSON_NAME QString("guide.json")

/**
 * @class SkyGuideMgr
 * @short This is the main class of the SkyGuide.
 * @author Marcos Cardinot <mcardinot@gmail.com>
 * @version 1.0
 */
class SkyGuideMgr : public QObject
{
    Q_OBJECT
public:
    SkyGuideMgr();
    virtual ~SkyGuideMgr();

    SkyGuideView* view() { return m_view; }
    SkyGuideWriter* getSkyGuideWriter() { return m_skyGuideWriter; }

    QDir getGuidesDir() { return m_guidesDir; }

    SkyGuideObject* buildSGOFromJson(const QString& jsonPath);
    SkyGuideObject* buildSGOFromZip(const QString& zipPath);
    void installSkyGuide(const QString& zipPath);
    bool uninstallSkyGuide(SkyGuideObject* obj);

private slots:
    void slotAddSkyGuide();
    void slotUninstallSkyGuide(int idx);

private:
    SkyGuideView* m_view;
    SkyGuideWriter* m_skyGuideWriter;
    QList<QObject*> m_skyGuideObjects;
    QDir m_guidesDir;

    void loadAllSkyGuideObjects();
    bool loadSkyGuideObject(SkyGuideObject* skyGuideObj);
    bool installDirIsWritable();
};

#endif // SKYGUIDEMGR_H
