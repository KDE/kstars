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

#include "geolocation.h"
#include "skypoint.h"
#include "infobox.h"

/**
  *@author Jason Harris
  */

class InfoBoxes : public QObject {
Q_OBJECT
public:
	InfoBoxes( int w, int h );
	~InfoBoxes();
	void resize( int w, int h ) { Width = w;  Height = h; }
	int width() const { return Width; }
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
private:
	int Width, Height;
	int GrabbedBox;
	bool Visible;
	QColor boxColor, grabColor;
	QPoint GrabPos;
	InfoBox *GeoBox, *FocusBox, *TimeBox;
};

#endif
