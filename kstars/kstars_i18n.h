/***************************************************************************
                          kstars_i18n.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu Dec 20 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/




#ifndef KSTARS_I18N_H
#define KSTARS_I18N_H


/**This is a dummy class whose sole purpose is to provide strings for translators
  *These strings are read from data files, but they must be present in a source file
  *or they won't be included in the template translation file.  It is redundant to define
  *these strings twice; perhaps we should just get rid of the data files and define the
  *strings here.  However, the place names database (Cities.dat) is quite large, and
  *contains many fields in addition to name strings.  It is much easier to manage this
  *information in a data file.
  *@author Jason Harris
  *@short dummy class for storing translatable strings from the data files.
  *@version 0.8
  */

class KStars_i18n {
public: 
	KStars_i18n();
	~KStars_i18n();
	void dummyFunction(void);
};

#endif
