/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#ifndef PLACEHOLDERPATH
#define PLACEHOLDERPATH

#include <lilxml.h>
#include "indi/indistd.h"

#include <QDebug>

class QString;
class SchedulerJob;

namespace Ekos {

class SequenceJob;

class PlaceholderPath
{
    public:
      PlaceholderPath();
      ~PlaceholderPath();

      void processJobInfo(SequenceJob *job, QString targetName);
      void addJob(SequenceJob *job, QString targetName);
      void constructPrefix(SequenceJob *job, QString &imagePrefix);
};

}

#endif /* ifndef PLACEHOLDERPATH */
