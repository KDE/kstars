/***************************************************************************
                          satellitecomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 14 July 2006
    copyright            : (C) 2006 by Jason Harris
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

#ifndef SATELLITECOMPONENT_H
#define SATELLITECOMPONENT_H

#include <QList>
#include <QPointF>
#include <QPen> 

#include "skycomponent.h"
#include "skyobjects/skypoint.h"

extern "C" {
#include "satlib/SatLib.h"
}

class SkyMap;
class SkyLabeler;


class SatelliteComponent : public SkyComponent
{
public:
    enum { NoLabel = 0, LeftEdgeLabel = 1, RightEdgeLabel = 2, UnknownLabel };

    /**@short Constructor
     * @p parent Pointer to the parent SkyComposite object
     */
    SatelliteComponent(SkyComposite* parent);

    virtual ~SatelliteComponent();

    /**@short Update the sky positions of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of the objects in this component.  However, the precession
     * and nutation must also be recomputed periodically.  Requests to do
     * so are sent through the doPrecess parameter.
     * @p num Pointer to the KSNumbers object
     * @note By default, the num parameter is NULL, indicating that 
     * Precession/Nutation computation should be skipped; this computation 
     * is only occasionally required.
     */
    virtual void update( KSNumbers *num=0 );
    
    virtual bool selected();
    virtual void draw( QPainter &psky );

    /**@short Initialize the component using a SPositionSat array */
    void initSat(const QString &name, SPositionSat *pSat[], int nsteps);

private:
    void drawLinesComponent( QPainter &psky );
    /* @short draw the label if any.  Is currently called at the bottom of
     * draw() but that call could be removed and it could be called
     * externally AFTER draw() has been called so draw() can set up the label
     * position candidates.
     */
    void drawLabels( QPainter& psky );

    /* @short This routine does two things at once.  It returns the QPointF
     * corresponding to pointList[i] and also computes the angle using
     * pointList[i] and pointList[i-1] therefore you MUST ensure that:
     *
     *       1 <= i < pointList.size().
     */
    QPointF angleAt( SkyMap* map, int i, double *angle );

    QList<SkyPoint*> pointList;
    int LabelPosition;
    QString Label;
    QPen Pen;

    SkyLabeler* m_skyLabeler;
    int m_iLeft, m_iRight, m_iTop, m_iBot; // the four label position candidates

    QStringList SatelliteNames;
    long double JulianDate;
    QList<double> JDList;
};

#endif
