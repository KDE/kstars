/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "culturelist.h"
#include "ksnumbers.h"
#include "skycomposite.h"
#include "skylabeler.h"
#include "skymesh.h"
#include "skyobject.h"

#include <QList>

#include <memory>

class QPolygonF;

class ArtificialHorizonComponent;
class ConstellationArtComponent;
class ConstellationBoundaryLines;
class ConstellationLines;
class ConstellationNamesComponent;
class ConstellationsArt;
class CatalogsComponent;
class DeepStarComponent;
class Ecliptic;
class Equator;
class EquatorialCoordinateGrid;
class FlagComponent;
class HorizontalCoordinateGrid;
class LocalMeridianComponent;
class HorizonComponent;
class KSPlanet;
class KSPlanetBase;
class MilkyWay;
class SatellitesComponent;
class SkyMap;
class SkyObject;
class SolarSystemComposite;
class StarComponent;
class SupernovaeComponent;
class TargetListComponent;
class HIPSComponent;
class TerrainComponent;

/**
 * @class SkyMapComposite
 *
 * SkyMapComposite is the root object in the object hierarchy of the sky map.
 * All requests to update, init, draw etc. will be done with this class.
 * The requests will be delegated to it's children.
 * The object hierarchy will created by adding new objects via addComponent().
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */
class SkyMapComposite : public QObject, public SkyComposite
{
    Q_OBJECT

  public:
    /**
         * Constructor
         * @p parent pointer to the parent SkyComponent
         */
    explicit SkyMapComposite(SkyComposite *parent = nullptr);

    virtual ~SkyMapComposite() override = default;

    void update(KSNumbers *num = nullptr) override;

    /**
         * @short Delegate planet position updates to the SolarSystemComposite
         *
         * Planet positions change over time, so they need to be recomputed
         * periodically, but not on every call to update().  This function
         * will recompute the positions of all solar system bodies except the
         * Earth's Moon, Jupiter's Moons AND Saturn Moons (because these objects' positions
         * change on a much more rapid timescale).
         * @p num Pointer to the KSNumbers object
         * @sa update()
         * @sa updateMoons()
         * @sa SolarSystemComposite::updatePlanets()
         */
    void updateSolarSystemBodies(KSNumbers *num) override;

    /**
         * @short Delegate moon position updates to the SolarSystemComposite
         *
         * Planet positions change over time, so they need to be recomputed
         * periodically, but not on every call to update().  This function
         * will recompute the positions of the Earth's Moon and Jupiter's four
         * Galilean moons.  These objects are done separately from the other
         * solar system bodies, because their positions change more rapidly,
         * and so updateMoons() must be called more often than updatePlanets().
         * @p num Pointer to the KSNumbers object
         * @sa update()
         * @sa updatePlanets()
         * @sa SolarSystemComposite::updateMoons()
         */
    void updateMoons(KSNumbers *num) override;

    /**
         * @short Delegate draw requests to all sub components
         * @p psky Reference to the QPainter on which to paint
         */
    void draw(SkyPainter *skyp) override;

    /**
         * @return the object nearest a given point in the sky.
         * @param p The point to find an object near
         * @param maxrad The maximum search radius, in Degrees
         * @note the angular separation to the matched object is returned
         * through the maxrad variable.
         */
    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    /**
         * @return the star nearest a given point in the sky.
         * @param p The point to find a star near
         * @param maxrad The maximum search radius, in Degrees
         * @note the angular separation to the matched star is returned
         * through the maxrad variable.
         */
    SkyObject *starNearest(SkyPoint *p, double &maxrad);

    /**
         * @short Search the children of this SkyMapComposite for
         * a SkyObject whose name matches the argument.
         *
         * The objects' primary, secondary and long-form names will
         * all be checked for a match.
         * @note Overloaded from SkyComposite.  In this version, we search
         * the most likely object classes first to be more efficient.
         * @p name the name to be matched
         * @return a pointer to the SkyObject whose name matches
         * the argument, or a nullptr pointer if no match was found.
         */
    SkyObject *findByName(const QString &name) override;

    /**
         * @return the list of objects in the region defined by skypoints
         * @param p1 first sky point (top-left vertex of rectangular region)
         * @param p2 second sky point (bottom-right vertex of rectangular region)
         */
    QList<SkyObject *> findObjectsInArea(const SkyPoint &p1, const SkyPoint &p2);

    bool addNameLabel(SkyObject *o);
    bool removeNameLabel(SkyObject *o);

    void reloadDeepSky();
    void reloadAsteroids();
    void reloadComets();
    void reloadCLines();
    void reloadCNames();
    void reloadConstellationArt();
#ifndef KSTARS_LITE
    FlagComponent *flags();
#endif
    SatellitesComponent *satellites();
    SupernovaeComponent *supernovaeComponent();
    ArtificialHorizonComponent *artificialHorizon();

    /** Getters for SkyComponents **/
    inline HorizonComponent *horizon() { return m_Horizon; }

    inline ConstellationBoundaryLines *constellationBoundary() { return m_CBoundLines; }
    inline ConstellationLines *constellationLines() { return m_CLines; }

    inline Ecliptic *ecliptic() { return m_Ecliptic; }
    inline Equator *equator() { return m_Equator; }

    inline EquatorialCoordinateGrid *equatorialCoordGrid()
    {
        return m_EquatorialCoordinateGrid;
    }
    inline HorizontalCoordinateGrid *horizontalCoordGrid()
    {
        return m_HorizontalCoordinateGrid;
    }
    inline LocalMeridianComponent *localMeridianComponent()
    {
        return m_LocalMeridianComponent;
    }

    inline ConstellationArtComponent *constellationArt() { return m_ConstellationArt; }

    inline SolarSystemComposite *solarSystemComposite() { return m_SolarSystem; }

    inline ConstellationNamesComponent *constellationNamesComponent() { return m_CNames; }

    inline CatalogsComponent *catalogsComponent() { return m_Catalogs; }

    inline MilkyWay *milkyWay() { return m_MilkyWay; }

    //Accessors for StarComponent
    SkyObject *findStarByGenetiveName(const QString name);
    void emitProgressText(const QString &message) override;
    QList<SkyObject *> &labelObjects() { return m_LabeledObjects; }

    const QList<SkyObject *> &constellationNames() const;
    const QList<SkyObject *> &stars() const;
    const QList<SkyObject *> &asteroids() const;
    const QList<SkyObject *> &comets() const;
    const QList<SkyObject *> &supernovae() const;
    QList<SkyObject *> planets();
    //    QList<SkyObject*> moons();

    const QList<SkyObject *> *getSkyObjectsList(SkyObject::TYPE t);

    KSPlanet *earth();
    KSPlanetBase *planet(int n);
    QStringList getCultureNames();
    QString getCultureName(int index);
    QString currentCulture();
    void setCurrentCulture(QString culture);
    bool isLocalCNames();

    inline TargetListComponent *getStarHopRouteList() { return m_StarHopRouteList; }
  signals:
    void progressText(const QString &message);

  private:
    QHash<int, QStringList> &getObjectNames() override;
    QHash<int, QVector<QPair<QString, const SkyObject *>>> &getObjectLists() override;

    std::unique_ptr<CultureList> m_Cultures;
    ConstellationBoundaryLines *m_CBoundLines{ nullptr };
    ConstellationNamesComponent *m_CNames{ nullptr };
    ConstellationLines *m_CLines{ nullptr };
    ConstellationArtComponent *m_ConstellationArt{ nullptr };
    EquatorialCoordinateGrid *m_EquatorialCoordinateGrid{ nullptr };
    HorizontalCoordinateGrid *m_HorizontalCoordinateGrid{ nullptr };
    LocalMeridianComponent *m_LocalMeridianComponent{ nullptr };
    CatalogsComponent *m_Catalogs{ nullptr };
    Equator *m_Equator{ nullptr };
    ArtificialHorizonComponent *m_ArtificialHorizon{ nullptr };
    Ecliptic *m_Ecliptic{ nullptr };
    HorizonComponent *m_Horizon{ nullptr };
    MilkyWay *m_MilkyWay{ nullptr };
    SolarSystemComposite *m_SolarSystem{ nullptr };
    StarComponent *m_Stars{ nullptr };
#ifndef KSTARS_LITE
    FlagComponent *m_Flags{ nullptr };
    HIPSComponent *m_HiPS{ nullptr };
    TerrainComponent *m_Terrain{ nullptr };
#endif
    TargetListComponent *m_ObservingList{ nullptr };
    TargetListComponent *m_StarHopRouteList{ nullptr };
    SatellitesComponent *m_Satellites{ nullptr };
    SupernovaeComponent *m_Supernovae{ nullptr };

    SkyMesh *m_skyMesh;
    std::unique_ptr<SkyLabeler> m_skyLabeler;

    KSNumbers m_reindexNum;

    QList<DeepStarComponent *> m_DeepStars;

    QList<SkyObject *> m_LabeledObjects;
    QHash<int, QStringList> m_ObjectNames;
    QHash<int, QVector<QPair<QString, const SkyObject *>>> m_ObjectLists;
    QHash<QString, QString> m_ConstellationNames;
};
