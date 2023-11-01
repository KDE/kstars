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
        typedef enum
        {
            PP_FORMAT,     // QString
            PP_SUFFIX,     // uint
            PP_DIRECTORY,  // QString
            PP_TARGETNAME, // QString
            PP_FRAMETYPE,  // CCDFrameType (uint)
            PP_DARKFLAT,   // bool
            PP_EXPOSURE,   // double
            PP_FILTER,     // QString
            PP_GAIN,       // double
            PP_OFFSET,     // double
            PP_PIERSIDE,   // ISD::Mount::PierSide (int)
            PP_TEMPERATURE // double
        } PathProperty;

        PlaceholderPath(const QString &seqFilename);
        PlaceholderPath();
        ~PlaceholderPath();

        /**
         * @brief processJobInfo loads the placeHolderPath with properties from the SequenceJob
         * @param sequence job to be processed
         */
        void processJobInfo(SequenceJob *job);

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
        QString generateSequenceFilename(const SequenceJob &job, bool local, const bool batch_mode,
                                 const int nextSequenceID, const QString &extension, const QString &filename,
                                 const bool glob = false, const bool gettingSignature = false) const;

        /**
         * @brief generateFilename performs the data for tag substituion in the filename
         * @param sequence job to be processed
         * @param local Generate local filename, otherwise, generate remote filename
         * @param batch_mode if true dateTime tag is returned with placeholders
         * @param nextSequenceID file sequence number count
         * @param extension filename extension
         * @param filename passed in with tags
         * @param glob used in batch mode
         * @return QString containing formatted filename
         *
         * This overload of the function supports calls from the indicamera class
         */
        QString generateOutputFilename(const bool local, const bool batch_mode, const int nextSequenceID, const QString &extension, const QString &filename,
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
         * @return a QStringList of the existing fileIDs
         */
        QList<int> getCompletedFileIds(const SequenceJob &job);

        /**
         * @brief getCompletedFiles provides the number of existing fileIDs
         * @param sequence job to be processed
         * @return number of existing fileIDs
         */
        int getCompletedFiles(const SequenceJob &job);

        /**
         * @brief checkSeqBoundary provides the ID to use for the next file
         * @param sequence job to be processed
         * @return number for the next fileIDs
         */
        int checkSeqBoundary(const SequenceJob &job);

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

        // shortcuts
        static bool isFilterEnabled(const QString format)
        {
            return format.contains("%F");
        }
        static bool isExpEnabled(const QString format)
        {
            return format.contains("%e");
        }
        static bool isTsEnabled(const QString format)
        {
            return format.contains("%D");
        }

private:
        // TODO use QVariantMap or QVariantList instead of passing this many args.
        QString generateFilenameInternal(const QMap<PathProperty, QVariant> &pathPropertyMap, const bool local, const bool batch_mode, const int nextSequenceID, const QString &extension,
                                 const QString &filename, const bool glob = false, const bool gettingSignature = false) const;

        QString getFrameType(CCDFrameType frameType) const
        {
            if (m_frameTypes.contains(frameType))
                return m_frameTypes[frameType];

            qWarning() << frameType << " not in " << m_frameTypes.keys();
            return "";
        }

        // properties that will be used for substitutions
        QMap<PathProperty, QVariant> m_PathPropertyMap;
        const QVariant getPathProperty(PathProperty prop) const
        {
            return m_PathPropertyMap[prop];
        }
        void setPathProperty(PathProperty prop, QVariant value)
        {
            m_PathPropertyMap[prop] = value;
        }

        QMap<CCDFrameType, QString> m_frameTypes;
        QFileInfo m_seqFilename;
};

}

