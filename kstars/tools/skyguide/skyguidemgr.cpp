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
#include <KMessageBox>
#include <kzip.h>

#include "kstars.h"
#include "skyguidemgr.h"

SkyGuideMgr::SkyGuideMgr()
        : m_view(new SkyGuideView())
{
    m_guidesDir = QDir(QStandardPaths::locate(QStandardPaths::DataLocation,
            "tools/skyguide/resources/guides", QStandardPaths::LocateDirectory));

    loadAllSkyGuideObjects();

    m_skyGuideWriter = new SkyGuideWriter(this, KStars::Instance());

    connect((QObject*)m_view->rootObject(), SIGNAL(addSkyGuide()), this, SLOT(slotAddSkyGuide()));
    connect((QObject*)m_view->rootObject(), SIGNAL(openWriter()), m_skyGuideWriter, SLOT(show()));
}

SkyGuideMgr::~SkyGuideMgr() {
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
            if (g.fileName() == JSON_NAME) {
                SkyGuideObject* s = buildSGOFromJson(g.absoluteFilePath());
                loadSkyGuideObject(s);
            }
        }
    }
    m_view->setModel(m_skyGuideObjects);
}

bool SkyGuideMgr::loadSkyGuideObject(SkyGuideObject* skyGuideObj) {
    if (!skyGuideObj || !skyGuideObj->isValid()) {
        return false;
    }

    // title is unique?
    foreach (QObject* obj, m_skyGuideObjects) {
        SkyGuideObject* curObj((SkyGuideObject*) obj);
        if (curObj->title() == skyGuideObj->title()) {
            QString caption = "Duplicated Title";
            QString message = "The SkyGuide title must be unique! \""
                            + skyGuideObj->title() + "\" is being used already.\n"
                            + "[1] creation date: " + curObj->creationDateStr()
                            + " - version: " + QString::number(curObj->version())
                            + "\n[2] creation date: " + skyGuideObj->creationDateStr()
                            + " - version: " + QString::number(skyGuideObj->version())
                            + "\nWould you like to overwrite the SkyGuide [1] or do you want to append a number on [2]?";

            qWarning()  << "SkyGuideMgr: Duplicated title! " << skyGuideObj->title();

            int ans = KMessageBox::warningYesNoCancel(0, message, caption,
                                                      KStandardGuiItem::overwrite(),
                                                      KStandardGuiItem::add());
            if (ans == KMessageBox::Yes) {
                uninstallSkyGuide(curObj);
            } else if (ans == KMessageBox::No) {
                skyGuideObj->setTitle(skyGuideObj->title() + QString::number(rand()));
            } else {
                return false;
            }
            break;
        }
    }
    m_skyGuideObjects.append(skyGuideObj);
    return true;
}

SkyGuideObject* SkyGuideMgr::buildSGOFromJson(const QString& jsonPath) {
    QFileInfo info(jsonPath);
    if (info.fileName() != JSON_NAME || !info.exists()) {
        QString message = "The JSON file is invalid or does not exist! \n" + jsonPath;
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return NULL;
    }

    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        QString message = "Couldn't open the JSON file! \n" + jsonPath;
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return NULL;
    }

    QJsonObject json(QJsonDocument::fromJson(jsonFile.readAll()).object());
    jsonFile.close();

    SkyGuideObject* s = new SkyGuideObject(info.absolutePath(), json.toVariantMap());
    if (!s->isValid()) {
        QString message = "The SkyGuide is invalid! \n" + jsonPath;
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return NULL;
    }
    return s;
}

SkyGuideObject* SkyGuideMgr::buildSGOFromZip(const QString& zipPath) {
    // try to open the SkyGuide archive
    KZip archive(zipPath);
    if (!archive.open(QIODevice::ReadOnly)) {
        QString message = "Unable to read the file! " + zipPath
                        + "\n Please, make sure it is a zip archive!";
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return NULL;
    }

    // check if this SkyGuide has a 'guide.json' file in the root
    const KArchiveDirectory *root = archive.directory();
    const KArchiveEntry *e = root->entry(JSON_NAME);
    if (!e) {
        QString message = "'" + JSON_NAME + "' not found! \n"
                        + "A SkyGuide must have a 'guide.json' in the root!";
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return NULL;
    }

    // create a clean /temp/skyguide dir
    QDir tmpDir = QDir::temp();
    if (tmpDir.cd("skyguide")) {    // already exists?
        tmpDir.removeRecursively(); // remove everything
    }
    tmpDir.mkdir("skyguide");
    tmpDir.cd("skyguide");

    //  copy files from archive to the temporary dir
    root->copyTo(tmpDir.absolutePath(), true);
    archive.close();

    return buildSGOFromJson(tmpDir.absoluteFilePath(JSON_NAME));
}

void SkyGuideMgr::installSkyGuide(const QString& zipPath) {
    // check if the installation dir is writable
    if (!QFileInfo(m_guidesDir.absolutePath()).isWritable()){
        QString message = "SkyGuideMgr: The installation directory must be writable! \n"
                        + m_guidesDir.absolutePath();
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return;
    }

    // try to load it!
    SkyGuideObject* obj = buildSGOFromZip(zipPath);
    if (!loadSkyGuideObject(obj)) {
        return;
    }

    // create a new folder on the installation dir to store this SkyGuide
    QString dirName = obj->title();
    int i = 0;
    while (!m_guidesDir.mkdir(dirName)) {
        dirName += QString::number(i);  // doesn't matter the number
        i++;
    }
    QString newPath = m_guidesDir.absolutePath() + QDir::separator() + dirName;

    // Copy the temp folder to the installation dir!
    // Users may want to change something else,
    // so we should leave the temp folder there for while
    // just to ensure that all image paths remain valid.
    QFileInfoList infoList = QDir(obj->path()).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    foreach (QFileInfo fileInfo, infoList) {
        QFile file(fileInfo.absoluteFilePath());
        QString newFilePath = newPath + QDir::separator() + fileInfo.fileName();
        file.copy(newFilePath);
    }

    // fix the SkyGuide path - now it's on the installation dir
    obj->setPath(newPath);

    // refresh view
    m_view->setModel(m_skyGuideObjects);
}

void SkyGuideMgr::slotAddSkyGuide() {
    // open QFileDialog - select the SkyGuide
    QString desktop = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
    QString path = QFileDialog::getOpenFileName(NULL, "Add SkyGuide", desktop, "Zip File (*.zip)");
    installSkyGuide(path);
}

bool SkyGuideMgr::uninstallSkyGuide(SkyGuideObject* obj) {
    if (!m_skyGuideObjects.removeOne(obj)) {
        QString message = "It seems that the SkyGuide \"" + obj->title()
                        + "\" is not installed already!";
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return true;
    }

    // we should remove only the skyguides which are on the installation dir!
    // so, make sure it's there!
    QDir root(obj->path());
    root.cdUp();
    if (root != m_guidesDir) {
        QString message = "Unable to remove the SkyGuide \"" + obj->title() + "\".\n"
                        + "It is not in the installation directory!\n"
                        + obj->path();
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return false;
    }

    if (!QDir(obj->path()).removeRecursively()) {
        QString message = "Unable to remove the SkyGuide \"" + obj->title() + "\".\n"
                        + "Please, check your permissions!\n"
                        + obj->path();
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideMgr: " << message;
        return false;
    }
    qDebug() << "SkyGuideMgr: The SkyGuide \"" + obj->title() + "\" on "
             << obj->path() << " was successfully removed!";
    return true;
}
