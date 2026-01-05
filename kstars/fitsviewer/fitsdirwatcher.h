/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QDir>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QMap>
#include <QSharedPointer>

/**
 * @brief The FITSDirWatcher holds routines for monitoring directories for new files. Currently only FITS files
 *        are supported.
 *
 *        1 or more directories are added to the watch list and the contents (files) are stored. When one or more files are
 *        added to a watched directory, they are emitted in a list to clients. Checks are made that the new files have
 *        been fully written before clients are notified so the client can simply go ahead and read the file. This
 *        checking process means that files are emitted one at a time.
 *
 * @author John Evans
 */
class FITSDirWatcher : public QObject
{
        Q_OBJECT

    public:
        explicit FITSDirWatcher(QObject *parent = nullptr);
        virtual ~FITSDirWatcher() override;

        /**
         * @brief Watch the input directories for new files
         * @param paths to watch in a map with matching channels
         * @return success (or not)
         */
        bool watchDirs(const QVector<QString> &paths);

        /**
         * @brief Stop watching directories
         */
        void stopWatching();

        /**
         * @brief Get the list of files in the watched directories
         * @return list of files
         */
        const QVector<LiveStackFile> getCurrentFiles() const
        {
            return m_CurrentFiles;
        };

    signals:
        // Signal the list of new files added since the previous signal (or since the directory watching began)
        void newFilesDetected(QDateTime timestamp, const QVector<LiveStackFile> &lsFile);

    private slots:
        // Something changed in the watched directory, so process for any new files
        void onDirChanged(const QString &path);

    private:
        // Check that the new files added are stable, i.e. it is not still being written
        void checkPendingFile(const QString &file);

        static constexpr int FILE_CHECK_INTERVAL_MS = 1000;     // Check files every 1s
        static constexpr int FILE_STABILITY_TIMEOUT_MS = 60000; // Keep checking for 60sec then give up

        struct PendingFile
        {
            QString filePath;
            qint64 initialSize;
            QDateTime lastModified;
            QDateTime firstDetected;
        };
        QMap<QString, PendingFile> m_PendingFiles;

        QSharedPointer<QFileSystemWatcher> m_Watcher;
        QVector<QString> m_WatchedPaths;
        QVector<LiveStackFile> m_CurrentFiles;
        QStringList m_NameFilters { "*.fits", "*.fits.fz", "*.fit", "*.fts" };
        QDir::Filters m_FilterFlags = QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks;
        QDir::SortFlags m_SortFlags = QDir::Time | QDir::Reversed;
        QMap<QString, QPair<int, QString>> m_FileToID;
        int m_NextID { 1 };
};
