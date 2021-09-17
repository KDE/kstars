/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarslite.h"

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

class FOVItem;

class TelescopeSymbolsItem;

/**
 * @class RootNode
 *
 * A QSGClipNode derived class used as a container for holding pointers to nodes and for clipping.
 * Upon construction RootNode generates all textures that are used by PointNode.
 *
 * KStars Lite has the following hierarchy:
 * 1. RootNode - the parent of all nodes that also acts as a clipping node
 * 2. SkyItem derived nodes that acts like SkyComponent in regular KStars
 * 3. SkyNode derived nodes that represent SkyObjects (similar to SkyComponent::draw())
 * 4. Simple nodes like EllipseNode or PointNode that draw basic geometry and stars
 *
 * @short A container for nodes that holds collection of textures for stars and provides clipping
 * @author Artem Fedoskin
 * @version 1.0
 */

class RootNode : public QSGClipNode
{
  public:
    RootNode();
    virtual ~RootNode();

    /**
     * @short returns cached texture from textureCache
     * @param size size of the star
     * @param spType spectral class
     * @return cached QSGTexture from textureCache
     */
    QSGTexture *getCachedTexture(int size, char spType);

    /** @short triangulates and sets new clipping polygon provided by Projection system */
    void updateClipPoly();

    /**
     * @short update positions of all child SkyItems
     * @param clearTextures true if textures for PointNodes should recreated (required when color scheme
     * is changed)
     */
    void update(bool clearTextures = false);

    /** Debug functions **/
    void testLeakDelete();
    void testLeakAdd();

    inline CometsItem *cometsItem() { return m_cometsItem; }

    inline LabelsItem *labelsItem() { return m_labelsItem; }

    inline StarItem *starItem() { return m_starItem; }
    /** @short initializes textureCache with cached images of stars from SkyMapLite */
    void genCachedTextures();

    inline TelescopeSymbolsItem *telescopeSymbolsItem() { return m_telescopeSymbols; }

  private:
    QVector<QVector<QSGTexture *>> m_textureCache;
    QVector<QVector<QSGTexture *>> m_oldTextureCache;
    SkyMapLite *m_skyMapLite { nullptr };

    QPolygonF m_clipPoly;
    QSGGeometry *m_clipGeometry { nullptr };

    StarItem *m_starItem { nullptr };
    DeepSkyItem *m_dsoItem { nullptr };

    PlanetsItem *m_planetsItem { nullptr };
    AsteroidsItem *m_asteroidsItem { nullptr };
    CometsItem *m_cometsItem { nullptr };

    ConstellationNamesItem *m_constelNamesItem { nullptr };
    LabelsItem *m_labelsItem { nullptr };
    ConstellationArtItem *m_artItem { nullptr };
    SatellitesItem *m_satItem { nullptr };
    SupernovaeItem *m_snovaItem { nullptr };

    HorizonItem *m_horizonItem { nullptr };
    LinesItem *m_linesItem { nullptr };
    EquatorItem *m_equator { nullptr };
    EclipticItem *m_ecliptic { nullptr };
    MilkyWayItem *m_MWItem { nullptr };

    SkyMapComposite *m_skyComposite { nullptr };
    SolarSystemComposite *m_solarSystem { nullptr };

    FOVItem *m_FOVItem { nullptr };
    TelescopeSymbolsItem *m_telescopeSymbols { nullptr };
};
