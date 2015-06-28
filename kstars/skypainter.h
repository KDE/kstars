/*
    SkyPainter: class for painting onto the sky for KStars
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef SKYPAINTER_H
#define SKYPAINTER_H

#include <QPainter>

#include "skycomponents/typedef.h"

class KSPlanetBase;
class DeepSkyObject;
class SkyPoint;
class SkyObject;
class SkyMap;
class SkipList;
class LineList;
class LineListLabel;
class Satellite;
class Supernova;
class ConstellationsArt;


/** @short Draws things on the sky, without regard to backend.
    This class serves as an interface to draw objects onto the sky without
    worrying about whether we are using a QPainter or OpenGL.
. */
class SkyPainter
{
public:
    /** @short Constructor.
        */
    SkyPainter();

    /**
     *@short Destructor
     */
    virtual ~SkyPainter();
    
    /** @short Set the pen of the painter **/
    virtual void setPen(const QPen& pen) = 0;

    /** @short Set the brush of the painter **/
    virtual void setBrush(const QBrush& brush) = 0;

    //FIXME: find a better way to do this.
    void setSizeMagLimit(float sizeMagLim);

    /** Begin painting.
        @note this function <b>must</b> be called before painting anything.
        @see end()
        */
    virtual void begin() =0;

    /** End and finalize painting.
        @note this function <b>must</b> be called after painting anything.
        @note it is not guaranteed that anything will actually be drawn until end() is called.
        @see begin();
        */
    virtual void end() =0;

    ////////////////////////////////////
    //                                //
    // SKY DRAWING FUNCTIONS:         //
    //                                //
    ////////////////////////////////////

    /** @short Draw the sky background */
    virtual void drawSkyBackground() = 0;

    /** @short Draw a line between points in the sky.
        @param a the first point
        @param b the second point
        @note this function will skip lines not on screen and clip lines
               that are only partially visible. */
    virtual void drawSkyLine(SkyPoint* a, SkyPoint* b) =0;

    /** @short Draw a polyline in the sky.
        @param list a list of points in the sky
        @param skipList a SkipList object used to control skipping line segments
        @param label a pointer to the label for this line
        @note it's more efficient to use this than repeated calls to drawSkyLine(),
               because it avoids an extra points->size() -2 projections.
        */
    virtual void drawSkyPolyline(LineList* list, SkipList *skipList = 0,
                                 LineListLabel *label = 0) =0;

    /** @short Draw a polygon in the sky.
        @param list a list of points in the sky
        @see drawSkyPolyline()
        */
    virtual void drawSkyPolygon(LineList* list) =0;

    /** @short Draw a point source (e.g., a star).
        @param loc the location of the source in the sky
        @param mag the magnitude of the source
        @param sp the spectral class of the source
        @return true if a source was drawn
        */
    virtual bool drawPointSource(SkyPoint *loc, float mag, char sp = 'A') =0;

    /** @short Draw a deep sky object
        @param obj the object to draw
        @param drawImage if true, try to draw the image of the object
        @return true if it was drawn
        */
    virtual bool drawDeepSkyObject(DeepSkyObject *obj, bool drawImage = false) =0;

    /** @short Draw a planet
        @param planet the planet to draw
        @return true if it was drawn
        */
    virtual bool drawPlanet(KSPlanetBase *planet) =0;

    /** @short Draw the symbols for the observing list
        @param obs the oberving list
        */
    virtual void drawObservingList( const QList<SkyObject*>& obs ) = 0;

    /** @short Draw flags
        */
    virtual void drawFlags() = 0;

    /** @short Draw a satellite
        */
    virtual void drawSatellite( Satellite* sat ) = 0;

    /** @short Draw a Supernova
     */
    virtual bool drawSupernova(Supernova* sup) = 0;

    virtual void drawHorizon( bool filled, SkyPoint *labelPoint = 0, bool *drawLabel = 0) = 0;

    /** @short Get the width of a star of magnitude mag */
    float starWidth(float mag) const;


    /** @short Draw a ConstellationsArt object
        @param obj the object to draw
        @param drawConstellationImage if true, try to draw the image of the object
        @return true if it was drawn
        */
    virtual bool drawConstellationArtImage(ConstellationsArt *obj, bool drawConstellationImage = true) = 0;

protected:

    SkyMap *m_sm;

private:
    float m_sizeMagLim;
};

#endif // SKYPAINTER_H
