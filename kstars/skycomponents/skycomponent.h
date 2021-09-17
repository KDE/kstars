/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"
#include "typedef.h"

class QString;

class SkyObject;
class SkyPoint;
class SkyComposite;
class SkyPainter;

/**
 * @class SkyComponent
 * SkyComponent represents an object on the sky map. This may be a star, a planet or an imaginary
 * line like the equator.
 * The SkyComponent uses the composite design pattern. So if a new object type is needed, it has to
 * be derived from this class and must be added into the hierarchy of components.
 *
 * \section Overview Overview
 * KStars utilizes the Composite/Component design pattern to store all elements that must be
 * drawn on the SkyMap.  In this design pattern, elements to be drawn in the map are called
 * "Components", and they are hierarchically organized into groups called "Composites".
 * A Composite can contain other Composites, as well as Components. From an organizational
 * point of view, you can think of Composites and Components as being analogous to
 * "Directories" and "Files" in a computer filesystem.
 *
 * The advantage of this design pattern is that it minimizes code duplication and maximizes
 * modularity. There is a set of operations which all Components must perform periodically
 * (such as drawing on screen, or updating position), but each Component may perform its task
 * slightly differently. Thus, each Component can implement its own version of the functions,
 * and each Composite calls on its child Components to execute the desired function. For
 * example, if we wanted to draw all Components in the SkyMap, we would simply need to call
 * "skyComposite()->draw()". SkyComposite is the "top-level" Composite; its "draw()" function
 * simply calls draw() on each of its children. Its child Components will draw themselves,
 * while its child Composites will call "draw()" for each of *their* children.  Note that
 * we don't need to know *any* implementation details; when we need to draw the
 * elements, we just tell the top-level Composite to spread the word.
 *
 * \section ClassInheritance Class Inheritance
 * The base class in this directory is SkyComponent; all other classes are derived from it
 * (directly or indirectly). Its most important derivative is SkyComposite, the base classes
 * for all Composites.
 *
 * FIXME: There is no SingleComponent
 * From SkyComponent, we derive three important subclasses: SingleComponent, ListComponent,
 * and PointListComponent. SingleComponent represents a sky element consisting of a single
 * SkyObject, such as the Sun.  ListComponent represents a list of SkyObjects, such as the
 * Asteroids.  PointListComponent represents a list of SkyPoints (not SkyObjects), such as
 * the Ecliptic. Almost all other Components are derived from one of these three classes
 * (two Components are derived directly from SkyComponent).
 *
 * The Solar System bodies require some special treatment. For example, only solar system
 * bodies may have attached trails, and they move across the sky, so their positions must
 * be updated frequently. To handle these features, we derive SolarSystemComposite from
 * SkyComposite to be the container for all solar system Components. In addition, we derive
 * SolarSystemSingleComponent from SingleComponent, and SolarSystemListComponent from
 * ListComponent. These classes simply add functions to handle Trails, and to update the
 * positions of the bodies. Also, they contain KSPlanetBase objects, instead of SkyObjects.
 *
 * The Sun, Moon and Pluto each get a unique Component class, derived from
 * SolarSystemSingleComponent (this is needed because each of these bodies uses a unique
 * class derived from KSPlanetBase: KSSun, KSMoon, and KSPluto). The remaining major
 * planets are each represented with a PlanetComponent object, which is also derived from
 * SolarSystemSingleComponent. Finally, we have AsteroidsComponent and CometsComponent,
 * each of which is derived from SolarSystemListComponent.
 *
 * Next, we have the Components derived from PointListComponent. These Components represent
 * imaginary guide lines in the sky, such as constellation boundaries or the coordinate grid.
 * They are listed above, and most of them don't require further comment. The GuidesComposite
 * contains two sub-Composites: CoordinateGridComposite, and ConstellationLinesComposite.
 * Both of these contain a number of PointListComponents: CoordinateGridComposite contains one
 * PointsListComponent for each circle in the grid, and ConstellationLineComposite contains
 * one PointsListComponent for each constellation (representing the "stick figure" of the
 * constellation).
 *
 * Finally, we have the Components which don't inherit from either SingleComponent,
 * ListComponent, or PointListComponent: PlanetMoonsComponent inherits from SkyComponent
 * directly, because the planet moons have their own class (PlanetMoons) not derived
 * directly from a SkyPoint.
 *
 * \section StarComponent DeepStarComponent and StarComponent
 * StarComponent and DeepStarComponent manage stars in KStars. StarComponent maintains a
 * QVector of instances of DeepStarComponents and takes care of calling their draw routines
 * etc. The machinery for handling stars is documented in great detail in \ref Stars
 *
 * @author Thomas Kabelmann
 */
class SkyComponent
{
  public:
    /**
     * @short Constructor
     * @p parent pointer to the parent SkyComposite
     */
    explicit SkyComponent(SkyComposite *parent = nullptr);

    virtual ~SkyComponent() = default;

    /**
     * @short Draw the object on the SkyMap
     * @p skyp a pointer to the SkyPainter to use
     */
    virtual void draw(SkyPainter *skyp) = 0;

    /** @short Draw trails for objects. */
    virtual void drawTrails(SkyPainter *skyp);

    /**
     * @short Update the sky position(s) of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of its member object(s).  However, the precession and
     * nutation must also be recomputed periodically.
     * @p num Pointer to the KSNumbers object
     * @sa SingleComponent::update()
     * @sa ListComponent::update()
     * @sa ConstellationBoundaryComponent::update()
     */
    virtual void update(KSNumbers *) {}
    virtual void updateSolarSystemBodies(KSNumbers *) {}
    virtual void updateMoons(KSNumbers *) {}

    /** @return true if component is to be drawn on the map. */
    virtual bool selected() { return true; }

    /** @return Parent of component. If there is no parent returns nullptr. */
    SkyComposite *parent() { return m_parent; }

    /**
     * @short Search the children of this SkyComponent for
     * a SkyObject whose name matches the argument
     * @p name the name to be matched
     * @return a pointer to the SkyObject whose name matches
     * the argument, or a nullptr pointer if no match was found.
     * @note This function simply returns the nullptr pointer; it
     * is reimplemented in various sub-classes
     */
    virtual SkyObject *findByName(const QString &name);

    /**
     * @short Searches the region(s) and appends the SkyObjects found to the list of sky objects
     *
     * Look for a SkyObject that is in one of the regions
     * If found, then append to the list of sky objects
     * @p list list of SkyObject to which matching list has to be appended to
     * @p region defines the regions in which the search for SkyObject should be done within
     * @return void
     * @note This function simply returns; it is
     * reimplemented in various sub-classes.
     */
    virtual void objectsInArea(QList<SkyObject *> &list, const SkyRegion &region);

    /**
     * @short Find the SkyObject nearest the given SkyPoint
     *
     * Look for a SkyObject that is nearer to point p than maxrad.
     * If one is found, then maxrad is reset to the separation of the new nearest object.
     * @p p pointer to the SkyPoint to search around
     * @p maxrad reference to current search radius in degrees
     * @return a pointer to the nearest SkyObject
     * @note This function simply returns a nullptr pointer; it is
     * reimplemented in various sub-classes.
     */
    virtual SkyObject *objectNearest(SkyPoint *p, double &maxrad);

    /**
     * @short Emit signal about progress.
     *
     * @sa SkyMapComposite::emitProgressText
     */
    virtual void emitProgressText(const QString &message);

    inline QHash<int, QStringList> &objectNames() { return getObjectNames(); }

    inline QStringList &objectNames(int type) { return getObjectNames()[type]; }

    inline QHash<int, QVector<QPair<QString, const SkyObject *>>> &objectLists() { return getObjectLists(); }

    inline QVector<QPair<QString, const SkyObject *>> &objectLists(int type) { return getObjectLists()[type]; }

    void removeFromNames(const SkyObject *obj);
    void removeFromLists(const SkyObject *obj);

  private:
    virtual QHash<int, QStringList> &getObjectNames();
    virtual QHash<int, QVector<QPair<QString, const SkyObject *>>> &getObjectLists();

    // Disallow copying and assignment
    SkyComponent(const SkyComponent &);
    SkyComponent &operator=(const SkyComponent &);

    /// Parent of sky component.
    SkyComposite *m_parent;
};
