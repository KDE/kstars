/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsdirwatcher.h"
#include <fits_debug.h>
#include <QTimer>

FITSDirWatcher::FITSDirWatcher(QObject *parent) : QObject(parent)
{
    m_Watcher.reset(new QFileSystemWatcher());

    // Connect the directory changed signal to our slot
    connect(m_Watcher.get(), &QFileSystemWatcher::directoryChanged, this, &FITSDirWatcher::onDirChanged);
}

FITSDirWatcher::~FITSDirWatcher()
{
}

// Start watching the specified directory
bool FITSDirWatcher::watchDir(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
    {
        qCDebug(KSTARS_FITS) << QString("Directory %1 does not exist").arg(path);
        return false;
    }

    // Store the current files in the directory - oldest first
    QStringList files = dir.entryList(m_NameFilters, m_FilterFlags, m_SortFlags);
    for (const QString &file : files)
        m_CurrentFiles.push_front(dir.absoluteFilePath(file));

    // Add the path to the watcher
    m_WatchedPath = path;
    return m_Watcher->addPath(path);
}

// Stop watching the current directory
void FITSDirWatcher::stopWatching()
{
    if (!m_WatchedPath.isEmpty())
    {
        m_Watcher->removePath(m_WatchedPath);
        m_WatchedPath.clear();
        m_CurrentFiles.clear();
        m_PendingFiles.clear();
    }
}

// Something happened (e.g. new file) to the watched directory
void FITSDirWatcher::onDirChanged(const QString &path)
{
    if (path != m_WatchedPath)
        return;

    QDir dir(path);
    QStringList newFileList;
    QStringList files = dir.entryList(m_NameFilters, m_FilterFlags, m_SortFlags);
    for (const QString &file : files)
        newFileList.push_back(dir.absoluteFilePath(file));

    // Find files that are in newFileList but not in m_currentFiles
    for (const QString &file : newFileList)
    {
        if (!m_CurrentFiles.contains(file) && !m_PendingFiles.contains(file))
        {
            // New file detected - start stability check
            QFileInfo fileInfo(file);
            PendingFile pending;
            pending.filePath = file;
            pending.initialSize = fileInfo.size();
            pending.lastModified = fileInfo.lastModified();
            pending.firstDetected = QDateTime::currentDateTime();

            m_PendingFiles[file] = pending;

            // Check stability after interval
            QTimer::singleShot(FILE_CHECK_INTERVAL_MS, this, [this, file]()
            {
                checkPendingFile(file);
            });
        }
    }
}

// Check new files for stability which we'll define as
// 1. Constant size
// 2. Last updated > 1 second ago
// 3. Able to get an exclusive lock on the file
void FITSDirWatcher::checkPendingFile(const QString &filePath)
{
    if (!m_PendingFiles.contains(filePath))
        return;

    PendingFile pending = m_PendingFiles[filePath];
    QFileInfo fileInfo(filePath);

    // Check if file still exists
    if (!fileInfo.exists())
    {
        m_PendingFiles.remove(filePath);
        return;
    }

    // Check for timeout
    QDateTime now = QDateTime::currentDateTime();
    if (pending.firstDetected.msecsTo(now) > FILE_STABILITY_TIMEOUT_MS)
    {
        qCWarning(KSTARS_FITS) << QString("File stability check timed out for %1 after %2s. Ignoring file...")
                                      .arg(filePath).arg(pending.firstDetected.msecsTo(now) / 1000.0);
        m_PendingFiles.remove(filePath);
        return;
    }

    // Check if file size modification time are stable
    bool sizeStable = (fileInfo.size() == pending.initialSize && fileInfo.size() > 0);
    bool timeStable = (fileInfo.lastModified() == pending.lastModified);
    bool isStable = sizeStable && timeStable;

    bool canLock = false;

    // Try and open for writing... if file still be written to this check may fail
    // So just another check on file stability
    if (isStable)
    {
        QFile file(filePath);
        canLock = file.open(QIODevice::ReadWrite);
        if (canLock)
            file.close();
    }

    if (isStable && canLock)
    {
        // File is stable - add to current files and signal
        m_CurrentFiles.append(filePath);
        m_PendingFiles.remove(filePath);
        qCDebug(KSTARS_FITS) << QString("File %1 stabilized after %2s").arg(filePath)
                                    .arg(pending.firstDetected.msecsTo(now) / 1000.0);
        emit newFilesDetected(QStringList() << filePath);
    }
    else
    {
        // File still changing - so wait and try again
        pending.initialSize = fileInfo.size();
        pending.lastModified = fileInfo.lastModified();
        m_PendingFiles[filePath] = pending;

        QTimer::singleShot(FILE_CHECK_INTERVAL_MS, this, [this, filePath]()
        {
            checkPendingFile(filePath);
        });
    }
}
