/*
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

#ifndef SKYGLPAINTER_H
#define SKYGLPAINTER_H

#include <skypainter.h>


class SkyGLPainter : public SkyPainter
{
public:
    SkyGLPainter(SkyMap* sm);
    virtual bool drawPlanetMoon(TrailObject* moon);
    virtual bool drawPlanet(KSPlanetBase* planet);
    virtual bool drawAsteroid(KSAsteroid* ast);
    virtual bool drawComet(KSComet* comet);
    virtual bool drawDeepSkyObject(DeepSkyObject* obj, bool drawImage = false);
    virtual bool drawPointSource(SkyPoint* loc, float mag, char sp = 'A');
    virtual void drawSkyPolygon(LineList* list);
    virtual void drawSkyPolyline(LineList* list, SkipList* skipList = 0, LineListLabel* label = 0);
    virtual void drawSkyLine(SkyPoint* a, SkyPoint* b);
    virtual void drawSkyBackground();
    virtual void end();
    virtual void begin();
    virtual void setBrush(const QBrush& brush);
    virtual void setPen(const QPen& pen);
private:
    void drawAsPoint(SkyPoint *p);
};

#endif // SKYGLPAINTER_H
