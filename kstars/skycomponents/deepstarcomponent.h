/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha Thomas Kabelmann <akarshsimha@gmail.com, thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

/**
 * @class DeepStarComponent
 * Stores and manages unnamed stars, most of which are dynamically loaded into memory.
 *
 * @author Akarsh Simha
 * @note Much of the code here is copied from class StarComponent authored by Thomas Kabelmann
 * @version 0.1
 */

#include "binfilehelper.h"
#include "ksnumbers.h"
#include "listcomponent.h"
#include "starblockfactory.h"
#include "skyobjects/deepstardata.h"
#include "skyobjects/stardata.h"

class SkyLabeler;
class SkyMesh;
class StarBlockFactory;
class StarBlockList;
class StarObject;

class DeepStarComponent : public ListComponent
{
#ifdef KSTARS_LITE
    friend class DeepStarItem; //Needs access to staticStars and buch of other facilities
#endif

  public:
    DeepStarComponent(SkyComposite *parent, QString fileName, float trigMag, bool staticstars = false);

    ~DeepStarComponent() override;

    //This function is empty; we need that so that the JiT update
    //is the only one being used.
    void update(KSNumbers *num) override;

    bool selected() override;

    void draw(SkyPainter *skyp) override;

    bool loadStaticStars();

    bool openDataFile();

    /**
     * @return true if this DeepStarComponent has static stars (that are not dynamically loaded)
     */
    inline bool hasStaticStars() const { return staticStars; }

    /**
     * @return return the estimated faint magnitude limit of this DeepStarComponent
     */
    float faintMagnitude() const { return m_FaintMagnitude; }

    /**
     * @param HDnum Henry-Draper catalog number of the desired star
     * @return A star matching the given Henry-Draper catalog number
     */
    StarObject *findByHDIndex(int HDnum);

    /**
     * @return Nearest star within maxrad of SkyPoint p, or nullptr if not found
     */
    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    inline bool fileOpen() const { return fileOpened; }

    inline BinFileHelper *getStarReader() { return &starReader; }

    bool verifySBLIntegrity();

    /**
     * @short Add to the given list, the stars from this component,
     * that lie within the specified circular aperture, and that are
     * brighter than the limiting magnitude specified.
     * @p center The center point of the aperture
     * @p radius The radius around the center point that defines the
     * aperture
     * @p maglim Optional parameter indicating the limiting magnitude.
     * If magnitude limit is numerically < -28, the limiting magnitude
     * is assumed to be the limiting magnitude of the catalog (i.e. no
     * magnitude limit)
     * @p list The list to operate on
     * @return false if the limiting magnitude is brighter than the
     * trigger magnitude of the DeepStarComponent
     */
    bool starsInAperture(QList<StarObject *> &list, const SkyPoint &center, float radius, float maglim = -29);

    // TODO: Find the right place for this method
    static void byteSwap(DeepStarData *stardata);
    static void byteSwap(StarData *stardata);

    static StarBlockFactory m_StarBlockFactory;

  private:
    SkyMesh *m_skyMesh { nullptr };
    KSNumbers m_reindexNum;

    float m_zoomMagLimit { 0 };
    /// Magnitude at which this catalog triggers
    float triggerMag { 0 };
    /// Limiting magnitude of the catalog currently loaded
    float m_FaintMagnitude { 0 };
    /// Indicates whether the file is opened or not
    bool fileOpened { false };
    unsigned long visibleStarCount { 0 };
    /// Maximum number of stars in any given trixel
    quint16 MSpT { 0 };

    // Time keeping variables
    long unsigned t_dynamicLoad { 0 };
    long unsigned t_drawUnnamed { 0 };
    long unsigned t_updateCache { 0 };

    QVector<std::shared_ptr<StarBlockList>> m_starBlockList;
    QHash<int, StarObject *> m_CatalogNumber;

    bool staticStars { false };

    // Stuff required for reading data
    DeepStarData deepstardata;
    StarData stardata;
    BinFileHelper starReader;
    QString dataFileName;
};
