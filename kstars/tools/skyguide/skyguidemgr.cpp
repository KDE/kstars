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

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QStandardPaths>
#include <kzip.h>

#include "skyguidemgr.h"

SkyGuideMgr::SkyGuideMgr()
    : m_view(new SkyGuideView())
{
    m_guidesDir = QDir(QStandardPaths::locate(QStandardPaths::DataLocation,
            "tools/skyguide/resources/guides", QStandardPaths::LocateDirectory));

    loadAllSkyGuideObjects();
    connect((QObject*)m_view->rootObject(), SIGNAL(addSkyGuide()), this, SLOT(slotAddSkyGuide()));
}

SkyGuideMgr::~SkyGuideMgr()
{
}

void SkyGuideMgr::loadAllSkyGuideObjects() {
    QDir root = m_guidesDir;
    QDir::Filters filters = QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks;
    root.setFilter(filters | QDir::Dirs);
    QFileInfoList guidesRoot = root.entryInfoList();
    foreach (QFileInfo r, guidesRoot) {
        QDir guideDir(r.filePath());
        guideDir.setFilter(filters | QDir::Files);
        QFileInfoList guideFiles = guideDir.entryInfoList();
        foreach (QFileInfo g, guideFiles) {
            if (g.fileName() == "guide.json") {
                loadSkyGuideObject(g.absolutePath(), g.fileName());
            }
        }
    }
    m_view->setModel(m_skyGuideObjects);
}

void SkyGuideMgr::loadSkyGuideObject(const QString& guidePath, const QString& filename)
{
    QString jsonPath = QDir::toNativeSeparators(guidePath + "/" + filename);
    QFile jsonFile(jsonPath);
    if (!jsonFile.exists()) {
        qWarning() << "SkyGuideMgr: The JSON file does not exist!" << jsonPath;
    } else if (!jsonFile.open(QIODevice::ReadOnly)) {
        qWarning() << "SkyGuideMgr: Couldn't open the JSON file!" << jsonPath;
    } else {
        QJsonObject json(QJsonDocument::fromJson(jsonFile.readAll()).object());
        SkyGuideObject* s = new SkyGuideObject(guidePath, json.toVariantMap());
        if (s->isValid()) {
            m_skyGuideObjects.append(s);
        } else {
            qWarning()  << "SkyGuideMgr: SkyGuide is invalid!" << jsonPath;
        }
    }
}

void SkyGuideMgr::slotAddSkyGuide() {
    if (!QFileInfo(m_guidesDir.absolutePath()).isWritable()){
        qWarning() << "SkyGuideMgr: Cannot write " << m_guidesDir.absolutePath();
        return;
    }

    QString desktop = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
    QString path = QFileDialog::getOpenFileName(NULL, "Add SkyGuide", desktop, "Zip File (*.zip)");

    KZip archive(path);
    if (!archive.open(QIODevice::ReadOnly)) {
        qWarning() << "SkyGuideMgr: Cannot open " << path;
        return;
    }

    const KArchiveDirectory *root = archive.directory();
    root->copyTo(m_guidesDir.absolutePath(), true);
    archive.close();

    loadAllSkyGuideObjects();
}
