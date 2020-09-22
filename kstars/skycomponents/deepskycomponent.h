/***************************************************************************
                          deepskycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/15/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include "skycomponent.h"
#include "skylabel.h"

class QPointF;

#ifdef KSTARS_LITE
class DeepSkyItem;
#endif
class DeepSkyObject;
class KSNumbers;
class SkyMap;
class SkyMesh;
class SkyPoint;

// NOTE: Although the following symbol has nothing to do with line
// number in any file, we use this name to keep consistency in naming
// conventions with StarComponent
#define MAX_LINENUMBER_MAG 90

typedef QVector<DeepSkyObject *> DeepSkyList;
typedef QHash<int, DeepSkyList *> DeepSkyIndex;

/**
 * @class DeepSkyComponent
 * Represents the deep sky objects separated by catalogs.
 * Custom Catalogs are a standalone component.
 * @note this Component is similar to ListComponent, but
 * the deep sky objects are stored in four separate QLists.
 * @author Thomas Kabelmann
 * @version 0.1
 */
class DeepSkyComponent : public SkyComponent
{
#ifdef KSTARS_LITE
    friend class DeepSkyItem;
#endif

  public:
    explicit DeepSkyComponent(SkyComposite *);

    ~DeepSkyComponent() override;

    void draw(SkyPainter *skyp) override;

    /**
     * @short draw all the labels in the prioritized LabelLists and then
     * clear the LabelLists.
     */
    void drawLabels();

    /**
     * @short Update the sky positions of this component.  FIXME -jbb does nothing now
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude) coordinates of the
     * objects in this component.  If the KSNumbers argument is not nullptr, this function also
     * recomputes precession and nutation for the date in KSNumbers.
     * @p num Pointer to the KSNumbers object
     * @note By default, the num parameter is nullptr, indicating that Precession/Nutation
     * computation should be skipped; this computation is only occasionally required.
     */
    void update(KSNumbers *num = nullptr) override;

    /**
     * @short Search the children of this SkyComponent for a SkyObject whose name matches the argument
     * @p name the name to be matched
     * @return a pointer to the SkyObject whose name matches the argument, or a nullptr pointer
     * if no match was found.
     */
    SkyObject *findByName(const QString &name) override;

    /**
     * @short Searches the region(s) and appends the SkyObjects found to the list of sky objects
     *
     * Look for a SkyObject that is in one of the regions
     * If found, then append to the list of sky objects
     * @p list list of SkyObject to which matching list has to be appended to
     * @p region defines the regions in which the search for SkyObject should be done within
     * @return void
     */
    void objectsInArea(QList<SkyObject *> &list, const SkyRegion &region) override;

    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    const QList<DeepSkyObject *> &objectList() const { return m_DeepSkyList; }

    bool selected() override;

    /**
     * @short Return the deep-sky limiting magnitude for the current zoom level
     *
     * The zoom level and various settings are determined by querying @class Options
     * @note Also used by CatalogComponent::draw
     */
    static double determineDeepSkyMagnitudeLimit(void);


  private:
    /**
     * @short Read the ngcic.dat deep-sky database.
     * Parse all lines from the deep-sky object catalog files. Construct a DeepSkyObject
     * from the data in each line, and add it to the DeepSkyComponent.
     *
     * Each line in the file is parsed according to column position:
     * @li 0        IC indicator [char]  If 'I' then IC object; if ' ' then NGC object
     * @li 1-4      Catalog number [int]  The NGC/IC catalog ID number
     * @li 6-8      Constellation code (IAU abbreviation)
     * @li 10-11    RA hours [int]
     * @li 13-14    RA minutes [int]
     * @li 16-19    RA seconds [float]
     * @li 21       Dec sign [char; '+' or '-']
     * @li 22-23    Dec degrees [int]
     * @li 25-26    Dec minutes [int]
     * @li 28-29    Dec seconds [int]
     * @li 31       Type ID [int]  Indicates object type; see TypeName array in kstars.cpp
     * @li 33-36    Type details [string] (not yet used)
     * @li 38-41    Magnitude [float] can be blank
     * @li 43-48    Major axis length, in arcmin [float] can be blank
     * @li 50-54    Minor axis length, in arcmin [float] can be blank
     * @li 56-58    Position angle, in degrees [int] can be blank
     * @li 60-62    Messier catalog number [int] can be blank
     * @li 64-69    PGC Catalog number [int] can be blank
     * @li 71-75    UGC Catalog number [int] can be blank
     * @li 77-END   Common name [string] can be blank
     * @return true if data file is successfully read.
     */
    void loadData();

    void clearList(QList<DeepSkyObject *> &list);

    void mergeSplitFiles();

    void drawDeepSkyCatalog(SkyPainter *skyp, bool drawObject, DeepSkyIndex *dsIndex, const QString &colorString,
                            bool drawImage = false);

    QList<DeepSkyObject *> m_DeepSkyList;

    QList<DeepSkyObject *> m_MessierList;
    QList<DeepSkyObject *> m_NGCList;
    QList<DeepSkyObject *> m_ICList;
    QList<DeepSkyObject *> m_OtherList;

    LabelList *m_labelList[MAX_LINENUMBER_MAG + 1];
    bool m_hideLabels { false };
    double m_zoomMagLimit { 0 };

    SkyMesh *m_skyMesh { nullptr };
    DeepSkyIndex m_DeepSkyIndex;
    DeepSkyIndex m_MessierIndex;
    DeepSkyIndex m_NGCIndex;
    DeepSkyIndex m_ICIndex;
    DeepSkyIndex m_OtherIndex;

    void appendIndex(DeepSkyObject *o, DeepSkyIndex *dsIndex, Trixel trixel);

    QHash<QString, DeepSkyObject *> nameHash;

    /** @short adds a label to the lists of labels to be drawn prioritized by magnitude. */
    void addLabel(const QPointF &p, DeepSkyObject *obj);
};
