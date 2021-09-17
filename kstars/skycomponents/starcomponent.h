/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksnumbers.h"
#include "listcomponent.h"
#include "skylabel.h"
#include "stardata.h"
#include "skyobjects/starobject.h"

#include <memory>

#ifdef KSTARS_LITE
class StarItem;
#endif

class KStarsSplash;
class BinFileHelper;
class DeepStarComponent;
class HighPMStarList;
class MeshIterator;
class SkyLabeler;
class SkyMesh;
class StarObject;
class StarBlockFactory;

#define MAX_LINENUMBER_MAG 90

/**
 * @class StarComponent
 *
 * @short Represents the stars on the sky map. For optimization reasons the stars are not
 * separate objects and are stored in a list.
 *
 * The StarComponent class manages all stars drawn in KStars. While it handles all stars
 * having names using its own member methods, it shunts the responsibility of unnamed stars
 * to the class 'DeepStarComponent', objects of which it maintains.
 *
 * @author Thomas Kabelmann
 * @author Akarsh Simha
 * @version 1.0
 */
class StarComponent : public ListComponent
{
#ifdef KSTARS_LITE
    friend class StarItem; //Needs access to faintMagnitude() and reindex()
#endif

  protected:
    StarComponent(SkyComposite *);

  public:
    ~StarComponent() override;

    // TODO: Desingletonize StarComponent
    /** @short Create an instance of StarComponent */
    static StarComponent *Create(SkyComposite *);

    /** @return the instance of StarComponent if already created, nullptr otherwise */
    static StarComponent *Instance() { return pinstance; }

    //This function is empty; we need that so that the JiT update
    //is the only one being used.
    void update(KSNumbers *num) override;

    bool selected() override;

    void draw(SkyPainter *skyp) override;

    /**
     * @short draw all the labels in the prioritized LabelLists and then clear the LabelLists.
     */
    void drawLabels();

    static float zoomMagnitudeLimit();

    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    virtual SkyObject *findStarByGenetiveName(const QString name);

    /**
     * @short Searches the region(s) and appends the SkyObjects found to the list of sky objects
     *
     * Look for a SkyObject that is in one of the regions
     * If found, then append to the list of sky objects
     * @p list list of SkyObject to which matching list has to be appended to
     * @p region defines the regions in which the search for SkyObject should be done within
     */
    void objectsInArea(QList<SkyObject *> &list, const SkyRegion &region) override;

    /**
     * @short Find stars by HD catalog index
     * @param HDnum HD Catalog Number of the star to find
     * @return If the star is a static star, a pointer to the star will be returned
     * If it is a dynamic star, a fake copy will be created that survives till
     * the next findByHDIndex() call. If no match was found, returns nullptr.
     */
    StarObject *findByHDIndex(int HDnum);


    /**
     * @short Append a star to the Object List. (including genetive name)
     *
     * Overrides ListComponent::appendListObject() to include genetive names of stars as well.
     */
    void appendListObject(SkyObject * object);

    /**
     * @short Add to the given list, the stars from this component, that lie within the
     * specified circular aperture, and that are brighter than the limiting magnitude specified.
     * @p center The center point of the aperture
     * @p radius The radius around the center point that defines the aperture
     * @p maglim Optional parameter indicating the limiting magnitude.
     * If magnitude limit is numerically < -28, the limiting magnitude
     * is assumed to be the limiting magnitude of the catalog (i.e. no magnitude limit)
     * @p list The list to operate on
     */
    void starsInAperture(QList<StarObject *> &list, const SkyPoint &center, float radius, float maglim = -29);

    // TODO: Make byteSwap a template method and put it in byteorder.h
    // It should ideally handle 32-bit, 16-bit fields and StarData and
    // DeepStarData fields
    static void byteSwap(StarData *stardata);

  private:
    /**
     * @short Read data for stars which will remain static in the memory
     *
     * This method reads data for named stars (stars having names, which are stored by
     * default in "namedstars.dat") into memory. These stars are always kept in memory,
     * as against 'deep' stars which are mostly loaded dynamically (KStars treats all
     * unnamed stars as 'deep' stars) into memory when required, depending on region
     * and magnitude limit. Once loading is successful, this method sets the starsLoaded flag to true
     */
    bool loadStaticData();

    /** @return the magnitude of the faintest star */
    float faintMagnitude() const;

    /** true if all stars(not only high PM ones) were reindexed else false**/
    bool reindex(KSNumbers *num);

    /** Adds a label to the lists of labels to be drawn prioritized by magnitude. */
    void addLabel(const QPointF &p, StarObject *star);

    void reindexAll(KSNumbers *num);

    /** Load available deep star catalogs */
    int loadDeepStarCatalogs();

    bool addDeepStarCatalogIfExists(const QString &fileName, float trigMag, bool staticstars = false);

    SkyMesh *m_skyMesh { nullptr };
    std::unique_ptr<StarIndex> m_starIndex;

    KSNumbers m_reindexNum;
    double m_reindexInterval { 0 };

    LabelList *m_labelList[MAX_LINENUMBER_MAG + 1];
    bool m_hideLabels { false };

    float m_zoomMagLimit { 0 };

    /// Limiting magnitude of the catalog currently loaded
    float m_FaintMagnitude { -5 };
    bool starsLoaded { false };
    /// Current limiting magnitude for visible stars
    float magLim { 0 };

    StarObject m_starObject;
    StarObject *focusStar { nullptr }; // This object is always drawn

    KStarsSplash *m_reindexSplash { nullptr };

    StarBlockFactory *m_StarBlockFactory { nullptr };

    QVector<HighPMStarList *> m_highPMStars;
    QHash<QString, SkyObject *> m_genName;
    QHash<int, StarObject *> m_HDHash;
    QVector<DeepStarComponent *> m_DeepStarComponents;

    /**
     * @struct starName
     * @brief Structure that holds star name information, to be read as-is from the
     * corresponding binary data file
     */
    typedef struct starName
    {
        char bayerName[8];
        char longName[32];
    } starName;

    StarData stardata;
    starName starname;

    static StarComponent *pinstance;
};
