/***************************************************************************
                          infopanel.h  -  Information Panel for KStars
                             -------------------
    begin                : Mon Jan 11 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**The KStars Info Panel.  This widget contains three rows of QLabels
	*containing status information about the simulation.  The first row
	*contains time-related labels: The local time, the universal time,
	*the sidereal time, and the Julian date.  The second row contains
	*focus-related labels: the RA/Dec coordinates, the Az/Alt coordinates,
	*and the name of the focused object.  The third row contains
	*Location-related labels: the longitude, the latitude, and the location's
	*name (city, province and country).
	*
	*The info panel provides methods to update the information in each row
	*separately.  It also provides methods to toggle the visibility of each row
	*(and the entire panel).
	*@short The KStars Info Panel
	*@author Mark Hollomon
	*@version 0.9
	*/
#ifndef KSTARS_INFOPANEL_H_
#define KSTARS_INFOPANEL_H_

#include <qframe.h>
#include <qlabel.h>
#include <qdatetime.h>

#include <klocale.h>

#include "geolocation.h"
#include "skypoint.h"


class InfoPanel : public QFrame {

	Q_OBJECT

	public:
	/**Constructor */
		InfoPanel(QWidget * parent, const char * name, const KLocale *loc, WFlags f = 0 );

	public slots:
	/**Update the Time-related labels according to the arguments.
		*/
		void timeChanged(QDateTime ut, QDateTime lt, QTime lst, long double julian);

	/**Update the Location-related labels.
		*/
		void geoChanged(const GeoLocation *geo);

	/**Update the focus coordinate-related labels.
		*/
		void focusCoordChanged(const SkyPoint *p);

	/**Update the focus object-related labels.
		*/
		void focusObjChanged(const QString &n);

	/**Toggle the visibility of the entire panel on/off.
		*/
		void showInfoPanel(bool showit);

	/**Toggle the visibility of the Time-related labels on/off.
		*/
		void showTimeBar(bool showit);

	/**Toggle the visibility of the Focus-related labels on/off.
		*/
		void showFocusBar(bool showit);

	/**Toggle the visibility of the Location-related labels on/off.
		*/
		void showLocationBar(bool showit);

	private:
		class pdata;
		pdata *pd;
};

#endif
