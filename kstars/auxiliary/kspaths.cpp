/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "auxiliary/kspaths.h"
#include <QFileInfo>
#include <QCoreApplication>

QString KSPaths::locate(QStandardPaths::StandardLocation location, const QString &fileName,
                        QStandardPaths::LocateOptions options)
{
    QString findings = QStandardPaths::locate(location, fileName, options);

    // If there was no result and we are running a test, if the location contains the app name, retry with installed name
    if (findings.isEmpty() && QStandardPaths::isTestModeEnabled()) switch(location)
    {
    case QStandardPaths::DataLocation:
    case QStandardPaths::AppDataLocation:
    //case QStandardPaths::AppLocalDataLocation:
        findings = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QDir("kstars").filePath(fileName), options);
        break;

    case QStandardPaths::CacheLocation:
        findings = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QDir("kstars").filePath(fileName), options);
        break;

    case QStandardPaths::AppConfigLocation:
        findings = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QDir("kstars").filePath(fileName), options);
        break;

    default: break;
    }

#ifdef ANDROID
    // If we are running Android, check the reserved-file area
    if (findings.isEmpty())
    {
        QFileInfo const rfile("/data/data/org.kde.kstars.lite/qt-reserved-files/share/kstars/" + fileName);
        if (rfile.exists())
            return rfile.filePath();
    }
#endif

    return findings;
}

QStringList KSPaths::locateAll(QStandardPaths::StandardLocation location, const QString &fileName,
                               QStandardPaths::LocateOptions options)
{
    QStringList findings = QStandardPaths::locateAll(location, fileName, options);

    // If there was no result and we are running a test, if the location contains the app name, retry with installed name
    if (findings.isEmpty() && QStandardPaths::isTestModeEnabled()) switch(location)
    {
    case QStandardPaths::DataLocation:
    case QStandardPaths::AppDataLocation:
    //case QStandardPaths::AppLocalDataLocation:
        findings = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QDir("kstars").filePath(fileName), options);
        break;

    case QStandardPaths::CacheLocation:
        findings = QStandardPaths::locateAll(QStandardPaths::GenericCacheLocation, QDir("kstars").filePath(fileName), options);
        break;

    case QStandardPaths::AppConfigLocation:
        findings = QStandardPaths::locateAll(QStandardPaths::GenericConfigLocation, QDir("kstars").filePath(fileName), options);
        break;

    default: break;
    }

#ifdef ANDROID
    // If we are running Android, check the reserved-file area
    if (findings.isEmpty())
    {
        QFileInfo const rfile("/data/data/org.kde.kstars.lite/qt-reserved-files/share/kstars/" + fileName);
        if (rfile.exists())
            return { rfile.filePath() };
    }
#endif

    return findings;
}

QString KSPaths::writableLocation(QStandardPaths::StandardLocation type)
{
    switch (type)
    {
    case QStandardPaths::GenericDataLocation:
    case QStandardPaths::GenericCacheLocation:
    case QStandardPaths::GenericConfigLocation:
        qWarning("Call to writableLocation without an application-based location.");
        break;

    default: break;
    }

    //Q_ASSERT_X(type != QStandardPaths::GenericDataLocation, __FUNCTION__, "GenericDataLocation is not writable.");
    //Q_ASSERT_X(type != QStandardPaths::GenericConfigLocation, __FUNCTION__, "GenericConfigLocation is not writable.");
    //Q_ASSERT_X(type != QStandardPaths::GenericCacheLocation, __FUNCTION__, "GenericCacheLocation is not writable.");

#ifdef Q_OS_ANDROID
    // No more writableLocation calls on Generic(Data|Config|Cache)Location anymore in KStars
#endif

#ifdef Q_OS_WIN
    // As long as the application name on Windows and Linux is the same, no change here
#endif

    return QStandardPaths::writableLocation(type);
}
