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


/**This is a dummy class whose sole purpose is to provide strings for 
  *translators.  The program reads these strings from data files, but 
  *that does not allow the strings to be included in the translation
  *template.  This file will make sure the strings are included.
  *
  *This really is a dummy class, it is not linked by the program.
  *
	*Redundant?  Yes.  Kludgy, too.  We could simply get rid of the data 
  *files and define these strings here.  However, the place names database
	*(Cities.dat) is quite large, and contains many numerical fields in 
  *addition to name strings.  It is much easier to manage this information
  *in an external data file.
  *@author Jason Harris
  *@short dummy class for storing translatable strings from the data files.
  *@version 0.9
  */

class KStars_i18n {
public: 
	KStars_i18n();
	~KStars_i18n();
	void dummyFunction(void);
};

#endif
