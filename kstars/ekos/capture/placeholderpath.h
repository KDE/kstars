/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#ifndef PLACEHOLDERPATH
#define PLACEHOLDERPATH

#include <lilxml.h>
#include "indi/indistd.h"

#include <QDebug>
#include <QFileInfo>

class QString;
class SchedulerJob;

namespace Ekos {

class SequenceJob;

class PlaceholderPath
{
    public:
      PlaceholderPath(QString seqFilename);
      PlaceholderPath();
      ~PlaceholderPath();

      void processJobInfo(SequenceJob *job, QString targetName);
      void addJob(SequenceJob *job, QString targetName);
      void constructPrefix(SequenceJob *job, QString &imagePrefix);
      void generateFilenameOld(
              const QString &format, bool batch_mode, QString *filename,
              QString fitsDir, QString seqPrefix, int nextSequenceID);
      void generateFilename(QString format, SequenceJob &job, QString targetName, bool batch_mode, int nextSequenceID, QString *filename) const;
      void generateFilename(QString format, bool tsEnabled, bool batch_mode,
              int nextSequenceID, QString *filename) const;
      void generateFilename(
              QString format, QString rawFilePrefix, bool filterEnabled, bool exposureEnabled,
              bool tsEnabled, QString filter, CCDFrameType frameType, double exposure, QString targetName,
              bool batch_mode, int nextSequenceID, QString *filename) const;

      void setGenerateFilenameSettings(const SequenceJob &job);
      void setSeqFilename(QString name) {
          m_seqFilename = name;
      }
      static QStringList remainingPlaceholders(QString filename);

    private:
      QString getFrameType(CCDFrameType frameType) const {
          if (m_frameTypes.contains(frameType)) {
              return m_frameTypes[frameType];
          }

          qWarning() << frameType << " not in " << m_frameTypes.keys();
          return "";
      }

      QMap<CCDFrameType, QString> m_frameTypes;
      QFileInfo m_seqFilename;
      QString m_RawPrefix;
      bool m_filterPrefixEnabled { false };
      bool m_expPrefixEnabled { false };
      QString m_filter;
      CCDFrameType m_frameType { FRAME_LIGHT };
      double m_exposure { -1 };
      QString m_targetName;
};

}

#endif /* ifndef PLACEHOLDERPATH */
