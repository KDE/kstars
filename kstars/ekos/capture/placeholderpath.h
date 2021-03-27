/***************************************************************************
                     placeholderpath.h  -  KStars Ekos
                             -------------------
    begin                : Tue 19 Jan 2021 15:06:21 CDT
    copyright            : (c) 2021 by Kwon-Young Choi
    email                : kwon-young.choi@hotmail.fr
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


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
      void generateFilename(
              const QString &format, bool batch_mode, QString *filename,
              QString fitsDir, QString seqPrefix, int nextSequenceID);
      void generateFilename(QString format, SequenceJob &job, QString targetName, bool batch_mode, int nextSequenceID, QString *filename);

    private:
      const QString getFrameType(CCDFrameType frameType) {
          if (m_frameTypes.contains(frameType)) {
              return m_frameTypes[frameType];
          }

          qWarning() << frameType << " not in " << m_frameTypes.keys();
          return "";
      }

      const QMap<CCDFrameType, QString> m_frameTypes;
      QFileInfo m_seqFilename;
};

}

#endif /* ifndef PLACEHOLDERPATH */
