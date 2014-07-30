/***************************************************************************
                          skyguideslistview.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/04/07
    copyright            : (C) 2014 by Gioacchino Mazzurco
    email                : gmazzurco89@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYMAPQMLADAPTER_H
#define SKYMAPQMLADAPTER_H

#include <QObject>

#include "Options.h"
#include "skymap.h"


class SkyMapQmlAdapter : public QObject
{
	Q_OBJECT

public:
	SkyMapQmlAdapter(SkyMap * skymap, QObject * parent = 0);

	Q_INVOKABLE double zoomFactor() const { return Options::zoomFactor(); };
	
	/**
	 * @short Set zoom factor.
	 * @param factor zoom factor
	 */
	Q_INVOKABLE void setZoomFactor(double factor) const { mSkyMap->setZoomFactor(factor); }
	
	/** Zoom in one step. */
	Q_INVOKABLE void zoomIn() { mSkyMap->slotZoomIn(); }

	/** Zoom out one step. */
	Q_INVOKABLE void zoomOut() { mSkyMap->slotZoomOut(); }

	/** Set default zoom. */
	Q_INVOKABLE void zoomDefault() { mSkyMap->slotZoomDefault(); }

	/**
	 * @short sets the focus point of the skymap, using ra/dec coordinates
	 *
	 * @note This function behaves essentially like the above function.
	 * It differs only in the data types of its arguments.
	 *
	 * @param ra the new right ascension
	 * @param dec the new declination
	 */
	Q_INVOKABLE void setFocusByCoordinates( double ra, double dec ) const;

	/** @short sets the central focus point of the sky map.
	 * @param name the name of the SkyObject the map should be centered on
	 */
	Q_INVOKABLE void setFocusBySkyObjectName(const QString & name) const;
	
	/** Recalculates the positions of objects in the sky, and then repaints the sky map.
     */
	Q_INVOKABLE void forceUpdate() const { mSkyMap->forceUpdate(); }

	

private:
	SkyMap * mSkyMap;
};

#endif // SKYMAPQMLADAPTER_H
