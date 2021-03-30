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
};

}

#endif /* ifndef PLACEHOLDERPATH */
