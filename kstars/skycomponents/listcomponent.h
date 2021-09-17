/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"

#include <QList>

class SkyComposite;
class SkyMap;

/**
 * @class ListComponent
 * An abstract parent class, to be inherited by SkyComponents that store a QList of SkyObjects.
 *
 * @author Jason Harris
 * @version 0.1
 */
class ListComponent : public SkyComponent
{
  public:
    explicit ListComponent(SkyComposite *parent);

    ~ListComponent() override;

    /**
     * @short Update the sky positions of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of the objects in this component.  If the KSNumbers*
     * argument is not nullptr, this function also recomputes precession and
     * nutation for the date in KSNumbers.
     * @p num Pointer to the KSNumbers object
     * @note By default, the num parameter is nullptr, indicating that
     * Precession/Nutation computation should be skipped; this computation
     * is only occasionally required.
     */
    void update(KSNumbers *num = nullptr) override;

    SkyObject *findByName(const QString &name) override;
    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    void clear();

    const QList<SkyObject *> &objectList() const { return m_ObjectList; }

    /**
     * @short Add an object to the Object list.
     *
     * This method is a handy wrapper, which automatically appends the given
     * SkyObject to m_ObjectList and inserts references with all common names (name,
     * name2, longname) into the m_ObjectHash QHash to enable a faster findbyname.
     */
    void appendListObject(SkyObject * object);

  protected:
    QList<SkyObject *> m_ObjectList;
    QHash<QString, SkyObject *> m_ObjectHash;
};
