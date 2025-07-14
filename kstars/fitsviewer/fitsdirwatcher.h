/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QDir>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QMap>
#include <QSharedPointer>

/**
 * @brief The FITSDirWatcher holds routines for monitoring a directory for new files. Currently only FITS files
 *        are supported.
 *
 *        A directory is added to the watch list and the contents (files) are stored. When one or more files are
 *        added to the directory, they are emitted in a list to clients. Checks are made that the new files have
 *        been fully written being clients are notified so the client can simply go ahead and read the file. This
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
     * @brief Watch the input directory for new files
     * @param path to watch
     * @return success (or not)
     */
    bool watchDir(const QString &path);

    /**
     * @brief Stop watching
     */
    void stopWatching();

    /**
     * @brief Get the list of files in the watched directory
     * @return list of files
     */
    const QStringList getCurrentFiles() const
    {
        return m_CurrentFiles;
    }

  signals:
    // Signal the list of new files added since the previous signal (or since the directory watching began)
    void newFilesDetected(const QStringList &filePaths);

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
    QString m_WatchedPath;
    QStringList m_StableFiles;
    QStringList m_CurrentFiles;
    QStringList m_NameFilters { "*.fits", "*.fits.fz", "*.fit", "*.fts" };
    QDir::Filters m_FilterFlags = QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks;
    QDir::SortFlags m_SortFlags = QDir::Time;
};
