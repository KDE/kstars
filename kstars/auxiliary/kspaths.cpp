/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "auxiliary/kspaths.h"
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>

namespace
{
// All call sites are behind KSTARS_BUILD_TESTING or Q_OS_MACOS guards, so a
// plain non-macOS production build does not reference this helper.
[[maybe_unused]] static bool isDataLocation(QStandardPaths::StandardLocation location)
{
    switch (location)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        case QStandardPaths::DataLocation:
#else
        case QStandardPaths::AppLocalDataLocation:
#endif
        case QStandardPaths::AppDataLocation:
        case QStandardPaths::GenericDataLocation:
            return true;

        default:
            return false;
    }
}
}

QString KSPaths::locate(QStandardPaths::StandardLocation location, const QString &fileName,
                        QStandardPaths::LocateOptions options)
{
    QString findings = QStandardPaths::locate(location, fileName, options);

    // If there was no result and we are running a test, if the location contains the app name, retry with installed name
    if (findings.isEmpty() && QStandardPaths::isTestModeEnabled())
    {
        switch(location)
        {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            case QStandardPaths::DataLocation:
#else
            case QStandardPaths::AppLocalDataLocation:
#endif
            case QStandardPaths::AppDataLocation:
                findings = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QDir("kstars").filePath(fileName), options);
                break;

            case QStandardPaths::CacheLocation:
                findings = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QDir("kstars").filePath(fileName), options);
                break;

            case QStandardPaths::AppConfigLocation:
            case QStandardPaths::GenericConfigLocation:
                findings = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QDir("kstars").filePath(fileName), options);
                break;

            default:
                break;
        }

#if defined(KSTARS_BUILD_TESTING)
        if (findings.isEmpty() && isDataLocation(location))
        {
            const QByteArray testDataDir = qgetenv("KSTARS_TEST_DATADIR");
            if (!testDataDir.isEmpty())
            {
                const QString candidate = QDir(QString::fromUtf8(testDataDir)).filePath(fileName);
                if (QFileInfo::exists(candidate))
                    return candidate;
            }
        }
#endif
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

#ifdef Q_OS_MACOS
    // Qt's AppLocalDataLocation on macOS includes Contents/Resources/ as a search root
    // but KStars data is installed into Contents/Resources/kstars/ (one level deeper).
    // Search that subdirectory explicitly so data files are found in the bundle without
    // requiring a copy to Application Support.
    if (findings.isEmpty() && isDataLocation(location))
    {
        const QString candidate = QDir(QCoreApplication::applicationDirPath()
                                       + "/../Resources/kstars/").filePath(fileName);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
#endif

    return findings;
}

QStringList KSPaths::locateAll(QStandardPaths::StandardLocation location, const QString &fileName,
                               QStandardPaths::LocateOptions options)
{
    QStringList findings = QStandardPaths::locateAll(location, fileName, options);

    // If there was no result and we are running a test, if the location contains the app name, retry with installed name
    if (findings.isEmpty() && QStandardPaths::isTestModeEnabled())
    {
        switch(location)
        {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            case QStandardPaths::DataLocation:
#else
            case QStandardPaths::AppLocalDataLocation:
#endif
            case QStandardPaths::AppDataLocation:
                findings = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QDir("kstars").filePath(fileName), options);
                break;

            case QStandardPaths::CacheLocation:
                findings = QStandardPaths::locateAll(QStandardPaths::GenericCacheLocation, QDir("kstars").filePath(fileName), options);
                break;

            case QStandardPaths::AppConfigLocation:
            case QStandardPaths::GenericConfigLocation:
                findings = QStandardPaths::locateAll(QStandardPaths::GenericConfigLocation, QDir("kstars").filePath(fileName), options);
                break;

            default:
                break;
        }

#if defined(KSTARS_BUILD_TESTING)
        if (findings.isEmpty() && isDataLocation(location))
        {
            const QByteArray testDataDir = qgetenv("KSTARS_TEST_DATADIR");
            if (!testDataDir.isEmpty())
            {
                const QString candidate = QDir(QString::fromUtf8(testDataDir)).filePath(fileName);
                if (QFileInfo::exists(candidate))
                    return { candidate };
            }
        }
#endif
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

#ifdef Q_OS_MACOS
    if (findings.isEmpty() && isDataLocation(location))
    {
        const QString candidate = QDir(QCoreApplication::applicationDirPath()
                                       + "/../Resources/kstars/").filePath(fileName);
        if (QFileInfo::exists(candidate))
            return { candidate };
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

        default:
            break;
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
