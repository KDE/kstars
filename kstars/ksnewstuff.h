/***************************************************************************
                          ksnewstuff.h  -  description
                             -------------------
    begin                : Wed 21 May 2004
    copyright            : (C) 2004 by Jason Harris
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

/**@class NewStuff class for KStars
	*Subclass of KNewStuff, which provides a GUI for downloading extra 
	*application data from the internet.  The KStars version is cutomized to 
	*parse the newly downloaded data and incorporate it immediately into 
	*the program.
	*@note This class is only compiled if the user has KDE >= 3.2.90, because
	*earlier versions of KDE did not have KNewStuff.
	*@author Jason Harris
	*@version 1.0
	*/

#ifndef KSNEWSTUFF_H
#define KSNEWSTUFF_H

#include <kdeversion.h>

#if KDE_IS_VERSION( 3, 2, 90 )

#include <klocale.h>
#include <kdebug.h>
#include <qobject.h>

#include <knewstuff/knewstuff.h>

class KDirWatch;
class KStars;

class KSNewStuff : public QObject, public KNewStuff
{
	Q_OBJECT
	public:
		KSNewStuff( QWidget *parent = 0 );
		bool install( const QString &fileName );

		bool createUploadFile( const QString & /*fileName*/ ) {
			kdDebug() << i18n( "Uploading data is not possible yet!" );
			return false;
		}

	public slots:
		void updateData( const QString &newFile );

 private:
	KDirWatch *kdw;
	KStars *ks;
};

#endif  // KDE >= 3.2.90
#endif  // KSNEWSTUFF_H
