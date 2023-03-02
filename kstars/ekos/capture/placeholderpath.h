/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <lilxml.h>
#include "indi/indistd.h"
#include <QDebug>
#include <QFileInfo>

class QString;
class SchedulerJob;

namespace Ekos
{

class SequenceJob;

class PlaceholderPath
{
    public:
        PlaceholderPath(const QString &seqFilename);
        PlaceholderPath();
        ~PlaceholderPath();

        /**
         * @brief processJobInfo loads the placeHolderPath with properties from the SequenceJob
         * @param sequence job to be processed
         * @param targetname name of the celestial target
         */
        void processJobInfo(SequenceJob *job, const QString &targetName);

        /**
         * @brief addjob creates the directory suffix for the SequenceJob
         * @param sequence job to be processed
         * @param targetname name of the celestial target
         */
        void addJob(SequenceJob *job, const QString &targetName);

        /**
         * @brief constructPrefix creates the prefix for the SequenceJob
         * @param sequence job to be processed
         * @param imagePrefix sequence job prefix string
         * @return QString containing the processed prefix string
         */
        QString constructPrefix(const SequenceJob *job, const QString &imagePrefix);

        /**
         * @brief generateFilename performs the data for tag substituion in the filename
         * @param sequence job to be processed
         * @param targetName name of the celestial target
         * @param local Generate local filename, otherwise, generate remote filename
         * @param batch_mode if true dateTime tag is returned with placeholders
         * @param nextSequenceID file sequence number count
         * @param extension filename extension
         * @param filename passed in with tags
         * @param glob used in batch mode
         * @return QString containing formatted filename
         *
         * This overload of the function supports calls from the capture class
         */
        QString generateFilename(const SequenceJob &job, const QString &targetName, bool local, const bool batch_mode,
                                 const int nextSequenceID, const QString &extension, const QString &filename,
                                 const bool glob = false, const bool gettingSignature = false) const;

        /**
         * @brief generateFilename performs the data for tag substituion in the filename
         * @param sequence job to be processed
         * @param batch_mode if true dateTime tag is returned with placeholders
         * @param nextSequenceID file sequence number count
         * @param extension filename extension
         * @param filename passed in with tags
         * @param glob used in batch mode
         * @return QString containing formatted filename
         *
         * This overload of the function supports calls from the indicamera class
         */
        QString generateFilename(const bool batch_mode, const int nextSequenceID, const QString &extension, const QString &filename,
                                 const bool glob = false, const bool gettingSignature = false) const;

        /**
         * @brief setGenerateFilenameSettings loads the placeHolderPath with settings from the passed job
         * @param sequence job to be processed
         */
        void setGenerateFilenameSettings(const SequenceJob &job);

        /**
         * @brief remainingPlaceholders finds placeholder tags in filename
         * @param filename string to be processed
         * @return a QStringList of the remaining placeholders
         */
        static QStringList remainingPlaceholders(const QString &filename);

        /**
         * @brief remainingPlaceholders provides a list of already existing fileIDs from passed sequence job
         * @param sequence job to be processed
         * @param targetName name of the celestial target
         * @return a QStringList of the existing fileIDs
         */
        QList<int> getCompletedFileIds(const SequenceJob &job, const QString &targetName);

        /**
         * @brief getCompletedFiles provides the number of existing fileIDs
         * @param sequence job to be processed
         * @param targetName name of the celestial target
         * @return number of existing fileIDs
         */
        int getCompletedFiles(const SequenceJob &job, const QString &targetName);

        /**
         * @brief checkSeqBoundary provides the ID to use for the next file
         * @param sequence job to be processed
         * @param targetName name of the celestial target
         * @return number for the next fileIDs
         */
        int checkSeqBoundary(const SequenceJob &job, const QString &targetName);

        /**
         * @brief defaultFormat provides a default format string
         * @param useFilter whether to include the filter in the format string
         * @param useExposure whether to include the exposure in the format string
         * @param useTimestamp whether to include the timestamp in the format string
         * @return the format string
         */
        static QString defaultFormat(bool useFilter, bool useExposure, bool useTimestamp);

        /**
         * @brief repairFilename is an emergency method used when an unexpected filename collision is detected.
         * @param filename the filename which already exists on disk.
         * @return Returns the repaired filename. If it was unable to repair, it returns the filename passed in.
         */
        static QString repairFilename(const QString &filename);

    private:
        // TODO use QVariantMap or QVariantList instead of passing this many args.
        QString generateFilename(const QString &directory, const QString &format, uint formatSuffix, const QString &rawFilePrefix,
                                 const bool isDarkFlat, const QString &filter, const CCDFrameType &frameType,
                                 const double exposure, const bool batch_mode, const int nextSequenceID, const QString &extension,
                                 const QString &filename, const bool glob = false, const bool gettingSignature = false) const;

        QString getFrameType(CCDFrameType frameType) const
        {
            if (m_frameTypes.contains(frameType))
                return m_frameTypes[frameType];

            qWarning() << frameType << " not in " << m_frameTypes.keys();
            return "";
        }

        QMap<CCDFrameType, QString> m_frameTypes;
        QFileInfo m_seqFilename;
        QString m_format;
        QString m_Directory;
        uint m_formatSuffix {3};
        bool m_tsEnabled { false };
        bool m_filterPrefixEnabled { false };
        bool m_expPrefixEnabled { false };
        bool m_DarkFlat {false};
        QString m_filter;
        CCDFrameType m_frameType { FRAME_LIGHT };
        double m_exposure { -1 };
        QString m_targetName;
};

}

