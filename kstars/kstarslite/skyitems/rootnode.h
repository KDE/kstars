/** *************************************************************************
                          rootnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 14/05/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef ROOTNODE_H_
#define ROOTNODE_H_
#include <QPolygonF>
#include <QSGClipNode>

class QSGTexture;
class SkyMapLite;

class StarItem;
class DeepSkyItem;

class PlanetsItem;
class AsteroidsItem;
class CometsItem;

class ConstellationNamesItem;
class LabelsItem;
class ConstellationArtItem;
class SatellitesItem;
class SupernovaeItem;

class LinesItem;
class HorizonItem;
class EquatorItem;
class EclipticItem;
class MilkyWayItem;

class SkyMapComposite;
class SolarSystemComposite;
#ifdef INDI_FOUND
class TelescopeSymbolsItem;
#endif

/** @class RootNode
 *
 * A QSGClipNode derived class used as a container for holding pointers to nodes and for clipping.
 * Upon construction RootNode generates all textures that are used by PointNode.
 *
 *@short A container for nodes that holds collection of textures for stars and provides clipping
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode : public QSGClipNode {
public:
    RootNode();
    virtual ~RootNode();
    /**
     * @short returns cached texture from textureCache
     * @param size size of the star
     * @param spType spectral class
     * @return cached QSGTexture from textureCache
     */
    QSGTexture* getCachedTexture(int size, char spType);

    /**
     * @short triangulates and sets new clipping polygon provided by Projection system
     */
    void updateClipPoly();

    /**
     * @short update positions of all child SkyItems
     */
    void update();

    //Debug
    void testLeakDelete();
    void testLeakAdd();

    inline CometsItem *cometsItem() { return m_cometsItem; }

    inline LabelsItem *labelsItem() { return m_labelsItem; }

    inline StarItem *starItem() { return m_starItem; }

#ifdef INDI_FOUND
    inline TelescopeSymbolsItem *telescopeSymbolsItem() { return m_telescopeSymbols; }
#endif
private:
    /**
     * @short initializes textureCache with cached images of stars from SkyMapLite
     */
    void genCachedTextures();

    QVector<QVector<QSGTexture *>> m_textureCache;
    SkyMapLite *m_skyMapLite;

    QPolygonF m_clipPoly;
    QSGGeometry *m_clipGeometry;

    StarItem *m_starItem;
    DeepSkyItem *m_dsoItem;

    PlanetsItem *m_planetsItem;
    AsteroidsItem *m_asteroidsItem;
    CometsItem *m_cometsItem;

    ConstellationNamesItem *m_constelNamesItem;
    LabelsItem *m_labelsItem;
    ConstellationArtItem *m_artItem;
    SatellitesItem *m_satItem;
    SupernovaeItem *m_snovaItem;

    HorizonItem *m_horizonItem;
    LinesItem *m_linesItem;
    EquatorItem *m_equator;
    EclipticItem *m_ecliptic;
    MilkyWayItem *m_MWItem;

    SkyMapComposite *m_skyComposite;
    SolarSystemComposite *m_solarSystem;
#ifdef INDI_FOUND
    TelescopeSymbolsItem *m_telescopeSymbols;
#endif
};
#endif
