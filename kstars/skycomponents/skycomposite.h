/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"

#include <QList>
#include <QMap>

class KSNumbers;

/**
 * @class SkyComposite
 * SkyComposite is a kind of container class for SkyComponent objects. The SkyComposite is
 * responsible for distributing calls to functions like draw() and update() to its children,
 * which can be SkyComponents or other SkyComposites with their own children. This is based
 * on the "composite/component" design pattern.
 *
 * Performance tuning: Sometimes it will be better to override a virtual function and do
 * the work in the composite instead of delegating the request to all sub components.
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */
class SkyComposite : public SkyComponent
{
  public:
    /**
     * @short Constructor
     * @p parent pointer to the parent SkyComponent
     */
    explicit SkyComposite(SkyComposite *parent = nullptr);

    /** *@short Destructor */
    ~SkyComposite() override;

    /**
     * @short Delegate draw requests to all sub components
     * @p psky Reference to the QPainter on which to paint
     */
    void draw(SkyPainter *skyp) override;

    /**
     * @short Delegate update-position requests to all sub components
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude) coordinates.
     * However, the precession and nutation must also be recomputed periodically. Requests to
     * do so are sent through the doPrecess parameter.
     * @p num Pointer to the KSNumbers object
     * @sa updatePlanets()
     * @sa updateMoons()
     * @note By default, the num parameter is nullptr, indicating that Precession/Nutation
     * computation should be skipped; this computation is only occasionally required.
     */
    void update(KSNumbers *num = nullptr) override;

    /**
     * @short Add a new sub component to the composite
     * @p comp Pointer to the SkyComponent to be added
     * @p priority A priority ordering for various operations on the list of all sky components
     * (notably objectNearest())
     */
    void addComponent(SkyComponent *comp, int priority = 1024);

    /**
     * @short Remove a sub component from the composite
     * @p comp Pointer to the SkyComponent to be removed.
     */
    void removeComponent(SkyComponent *const comp);

    /**
     * @short Search the children of this SkyComposite for a SkyObject whose name matches
     * the argument.
     *
     * The objects' primary, secondary and long-form names will all be checked for a match.
     * @p name the name to be matched
     * @return a pointer to the SkyObject whose name matches
     * the argument, or a nullptr pointer if no match was found.
     */
    SkyObject *findByName(const QString &name) override;

    /**
     * @short Identify the nearest SkyObject to the given SkyPoint, among the children of
     * this SkyComposite
     * @p p pointer to the SkyPoint around which to search.
     * @p maxrad reference to current search radius
     * @return a pointer to the nearest SkyObject
     */
    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    QList<SkyComponent *> components() { return m_Components.values(); }

    QMap<int, SkyComponent *> &componentsWithPriorities() { return m_Components; }

  private:
    QMap<int, SkyComponent *> m_Components;
};
