/***************************************************************************
                          infoboxes.h  -  description
                             -------------------
    begin                : Wed Jun 5 2002
    copyright            : (C) 2002 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef INFOBOXES_H
#define INFOBOXES_H

#include <qobject.h>
#include <qcolor.h>
#include <qdatetime.h>
#include <qevent.h>
#include <qpoint.h>
#include <kdebug.h>

#include "geolocation.h"
#include "skypoint.h"
#include "infobox.h"

/**
  *@author Jason Harris
  */

class InfoBoxes : public QObject {
Q_OBJECT
public:
	InfoBoxes( int w, int h,
			int tx=0, int ty=0, bool tshade=false,
			int gx=0, int gy=600, bool gshade=false,
			int fx=600, int fy=0, bool fshade=false,
			QColor colorText=QColor("white"),
			QColor colorGrab=QColor("red"),
			QColor colorBG=QColor("black") );

	InfoBoxes( int w, int h,
			QPoint tp, bool tshade,
			QPoint gp, bool gshade,
			QPoint fp, bool fshade,
			QColor colorText=QColor("white"),
			QColor colorGrab=QColor("red"),
			QColor colorBG=QColor("black") );

	~InfoBoxes();

	InfoBox *timeBox()  { return TimeBox; }
	InfoBox *geoBox()   { return GeoBox; }
	InfoBox *focusBox() { return FocusBox; }

	/**Resets the width and height parameters.  These usually reflect the size
		*of the Skymap widget (Skymap::resizeEvent() calls this function).
		*Will also reposition the infoboxes to fit the new size.  Infoboxes
		*that were along an edge will remain along thr edge.
		*/
	void resize( int w, int h );

	/**@returns the width of the region containing the infoboxes (usually the
		*width of the Skymap)
		*/
	int width() const { return Width; }

	/**@returns the height of the region containing the infoboxes (usually the
		*height of the Skymap)
		*/
	int height() const { return Height; }
	void drawBoxes( QPainter &p, QColor FGColor=QColor("white"),
			QColor grabColor=QColor("red"), QColor BGColor=QColor("black"),
			bool fillBG=false );
	bool grabBox( QMouseEvent *e );
	bool unGrabBox();
	bool dragBox( QMouseEvent *e );
	bool shadeBox( QMouseEvent *e );
	bool fixCollisions( InfoBox *target );
	bool isVisible() { return Visible; }

public slots:
	void setVisible( bool t ) { Visible = t; }
	void showTimeBox( bool t ) { TimeBox->setVisible( t ); }
	void showGeoBox( bool t ) { GeoBox->setVisible( t ); }
	void showFocusBox( bool t ) { FocusBox->setVisible( t ); }

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

/**Check if boxes are anchored with bottom or right border.
	@param resetToDefault reset all borders of boxes to false before checking borders.
	*/	
	void checkBorders(bool resetToDefault=true);

private:
	int Width, Height;
	int GrabbedBox;
	bool Visible;
	QColor boxColor, grabColor, bgColor;
	QPoint GrabPos;
	InfoBox *GeoBox, *FocusBox, *TimeBox;
};

#endif
