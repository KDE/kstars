/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha Thomas Kabelmann <akarshsimha@gmail.com, thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deepstarcomponent.h"

#include "byteorder.h"
#include "kstarsdata.h"
#include "Options.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "skymesh.h"
#include "skypainter.h"
#include "starblock.h"
#include "starcomponent.h"
#include "htmesh/MeshIterator.h"
#include "projections/projector.h"

#include <qplatformdefs.h>
#include <QtConcurrent>
#include <QElapsedTimer>

#include <kstars_debug.h>

#ifdef _WIN32
#include <windows.h>
#endif

DeepStarComponent::DeepStarComponent(SkyComposite *parent, QString fileName, float trigMag, bool staticstars)
    : ListComponent(parent), m_reindexNum(J2000), triggerMag(trigMag), m_FaintMagnitude(-5.0), staticStars(staticstars),
      dataFileName(fileName)
{
    fileOpened = false;
    openDataFile();
    if (staticStars)
        loadStaticStars();
    qCInfo(KSTARS) << "Loaded DSO catalog file: " << dataFileName;
}

DeepStarComponent::~DeepStarComponent()
{
    if (fileOpened)
        starReader.closeFile();
    fileOpened = false;
}

bool DeepStarComponent::loadStaticStars()
{
    FILE *dataFile;

    if (!staticStars)
        return true;
    if (!fileOpened)
        return false;

    dataFile = starReader.getFileHandle();
    rewind(dataFile);

    if (!starReader.readHeader())
    {
        qCCritical(KSTARS) << "Error reading header of catalog file " << dataFileName << ": " << starReader.getErrorNumber()
                           << ": " << starReader.getError();
        return false;
    }

    quint8 recordSize = starReader.guessRecordSize();

    if (recordSize != 16 && recordSize != 32)
    {
        qCCritical(KSTARS) << "Cannot understand catalog file " << dataFileName;
        return false;
    }

    //KDE_fseek(dataFile, starReader.getDataOffset(), SEEK_SET);
    QT_FSEEK(dataFile, starReader.getDataOffset(), SEEK_SET);

    qint16 faintmag;
    quint8 htm_level;
    quint16 t_MSpT;
    int ret = 0;

    ret = fread(&faintmag, 2, 1, dataFile);
    if (starReader.getByteSwap())
        faintmag = bswap_16(faintmag);
    ret = fread(&htm_level, 1, 1, dataFile);
    ret = fread(&t_MSpT, 2, 1, dataFile); // Unused
    if (starReader.getByteSwap())
        faintmag = bswap_16(faintmag);

    // TODO: Read the multiplying factor from the dataFile
    m_FaintMagnitude = faintmag / 100.0;

    if (htm_level != m_skyMesh->level())
        qCWarning(KSTARS) << "HTM Level in shallow star data file and HTM Level in m_skyMesh do not match. EXPECT TROUBLE!";

    // JM 2012-12-05: Breaking into 2 loops instead of one previously with multiple IF checks for recordSize
    // While the CPU branch prediction might not suffer any penalties since the branch prediction after a few times
    // should always gets it right. It's better to do it this way to avoid any chances since the compiler might not optimize it.
    if (recordSize == 32)
    {
        for (Trixel i = 0; i < m_skyMesh->size(); ++i)
        {
            Trixel trixel   = i;
            quint64 records = starReader.getRecordCount(i);
            std::shared_ptr<StarBlock> SB(new StarBlock(records));

            if (!SB.get())
                qCCritical(KSTARS) << "ERROR: Could not allocate new StarBlock to hold shallow unnamed stars for trixel "
                                   << trixel;

            m_starBlockList.at(trixel)->setStaticBlock(SB);

            for (quint64 j = 0; j < records; ++j)
            {
                bool fread_success = fread(&stardata, sizeof(StarData), 1, dataFile);

                if (!fread_success)
                {
                    qCCritical(KSTARS) << "ERROR: Could not read StarData structure for star #" << j << " under trixel #"
                                       << trixel;
                }

                /* Swap Bytes when required */
                if (starReader.getByteSwap())
                    byteSwap(&stardata);

                /* Initialize star with data just read. */
                StarObject *star;
#ifdef KSTARS_LITE
                star = &(SB->addStar(stardata)->star);
#else
                star = SB->addStar(stardata);
#endif
                if (star)
                {
                    //KStarsData* data = KStarsData::Instance();
                    //star->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                    //if( star->getHDIndex() != 0 )
                    if (stardata.HD)
                        m_CatalogNumber.insert(stardata.HD, star);
                }
                else
                {
                    qCCritical(KSTARS) << "CODE ERROR: More unnamed static stars in trixel " << trixel
                                       << " than we allocated space for!";
                }
            }
        }
    }
    else
    {
        for (Trixel i = 0; i < m_skyMesh->size(); ++i)
        {
            Trixel trixel   = i;
            quint64 records = starReader.getRecordCount(i);
            std::shared_ptr<StarBlock> SB(new StarBlock(records));

            if (!SB.get())
                qCCritical(KSTARS) << "Could not allocate new StarBlock to hold shallow unnamed stars for trixel "
                                   << trixel;

            m_starBlockList.at(trixel)->setStaticBlock(SB);

            for (quint64 j = 0; j < records; ++j)
            {
                bool fread_success = false;
                fread_success      = fread(&deepstardata, sizeof(DeepStarData), 1, dataFile);

                if (!fread_success)
                {
                    qCCritical(KSTARS) << "Could not read StarData structure for star #" << j << " under trixel #"
                                       << trixel;
                }

                /* Swap Bytes when required */
                if (starReader.getByteSwap())
                    byteSwap(&deepstardata);

                /* Initialize star with data just read. */
                StarObject *star;
#ifdef KSTARS_LITE
                star = &(SB->addStar(stardata)->star);
#else
                star = SB->addStar(deepstardata);
#endif
                if (star)
                {
                    //KStarsData* data = KStarsData::Instance();
                    //star->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                    //if( star->getHDIndex() != 0 )
                    if (stardata.HD)
                        m_CatalogNumber.insert(stardata.HD, star);
                }
                else
                {
                    qCCritical(KSTARS) << "CODE ERROR: More unnamed static stars in trixel " << trixel
                                       << " than we allocated space for!";
                }
            }
        }
    }

    return true;
}

bool DeepStarComponent::selected()
{
    return Options::showStars() && fileOpened;
}

bool openIndexFile()
{
    // TODO: Work out the details
    /*
    if( hdidxReader.openFile( "Henry-Draper.idx" ) )
        qDebug() << "Could not open HD Index file. Search by HD numbers for deep stars will not work.";
    */
    return 0;
}

//This function is empty for a reason; we override the normal
//update function in favor of JiT updates for stars.
void DeepStarComponent::update(KSNumbers *)
{
}

// TODO: Optimize draw, if it is worth it.
void DeepStarComponent::draw(SkyPainter *skyp)
{
#ifndef KSTARS_LITE
    if (!fileOpened)
        return;

#ifdef PROFILE_SINCOS
    long trig_calls_here      = -dms::trig_function_calls;
    long trig_redundancy_here = -dms::redundant_trig_function_calls;
    long cachingdms_bad_uses  = -CachingDms::cachingdms_bad_uses;
    dms::seconds_in_trig      = 0.;
#endif

#ifdef PROFILE_UPDATECOORDS
    StarObject::updateCoordsCpuTime = 0.;
    StarObject::starsUpdated        = 0;
#endif
    SkyMap *map       = SkyMap::Instance();
    KStarsData *data  = KStarsData::Instance();
    UpdateID updateID = data->updateID();

    //FIXME_FOV -- maybe not clamp like that...
    float radius = map->projector()->fov();
    if (radius > 90.0)
        radius = 90.0;

    if (m_skyMesh != SkyMesh::Instance() && m_skyMesh->inDraw())
    {
        printf("Warning: aborting concurrent DeepStarComponent::draw()");
    }
    bool checkSlewing = (map->isSlewing() && Options::hideOnSlew());

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars(checkSlewing && Options::hideStars());
    double hideStarsMag = Options::magLimitHideStar();

    //adjust maglimit for ZoomLevel
    //    double lgmin = log10(MINZOOM);
    //    double lgmax = log10(MAXZOOM);
    //    double lgz = log10(Options::zoomFactor());
    // TODO: Enable hiding of faint stars

    float maglim = StarComponent::zoomMagnitudeLimit();

    if (maglim < triggerMag)
        return;

    m_zoomMagLimit = maglim;

    m_skyMesh->inDraw(true);

    SkyPoint *focus = map->focus();
    m_skyMesh->aperture(focus, radius + 1.0, DRAW_BUF); // divide by 2 for testing

    MeshIterator region(m_skyMesh, DRAW_BUF);

    // If we are to hide the fainter stars (eg: while slewing), we set the magnitude limit to hideStarsMag.
    if (hideFaintStars && maglim > hideStarsMag)
        maglim = hideStarsMag;

    StarBlockFactory *m_StarBlockFactory = StarBlockFactory::Instance();
    //    m_StarBlockFactory->drawID = m_skyMesh->drawID();
    //    qDebug() << "Mesh size = " << m_skyMesh->size() << "; drawID = " << m_skyMesh->drawID();
    QElapsedTimer t;
    int nTrixels = 0;

    t_dynamicLoad = 0;
    t_updateCache = 0;
    t_drawUnnamed = 0;

    visibleStarCount = 0;

    t.start();

    // Mark used blocks in the LRU Cache. Not required for static stars
    if (!staticStars)
    {
        while (region.hasNext())
        {
            Trixel currentRegion = region.next();
            for (int i = 0; i < m_starBlockList.at(currentRegion)->getBlockCount(); ++i)
            {
                std::shared_ptr<StarBlock> prevBlock = ((i >= 1) ? m_starBlockList.at(currentRegion)->block(
                        i - 1) : std::shared_ptr<StarBlock>());
                std::shared_ptr<StarBlock> block     = m_starBlockList.at(currentRegion)->block(i);

                if (i == 0 && !m_StarBlockFactory->markFirst(block))
                    qCWarning(KSTARS) << "markFirst failed in trixel" << currentRegion;
                if (i > 0 && !m_StarBlockFactory->markNext(prevBlock, block))
                    qCWarning(KSTARS) << "markNext failed in trixel" << currentRegion << "while marking block" << i;
                if (i < m_starBlockList.at(currentRegion)->getBlockCount() &&
                        m_starBlockList.at(currentRegion)->block(i)->getFaintMag() < maglim)
                    break;
            }
        }
        t_updateCache = t.elapsed();
        region.reset();
    }

    while (region.hasNext())
    {
        ++nTrixels;
        Trixel currentRegion = region.next();

        // NOTE: We are guessing that the last 1.5/16 magnitudes in the catalog are just additions and the star catalog
        //       is actually supposed to reach out continuously enough only to mag m_FaintMagnitude * ( 1 - 1.5/16 )
        // TODO: Is there a better way? We may have to change the magnitude tolerance if the catalog changes
        // Static stars need not execute fillToMag

        // Safety check if the current region is in star block list
        if (currentRegion >= m_starBlockList.size())
            continue;

        if (!staticStars)
        {
            m_starBlockList.at(currentRegion)->fillToMag(maglim);
        }

        //        if (!staticStars && !m_starBlockList.at(currentRegion)->fillToMag(maglim) &&
        //            maglim <= m_FaintMagnitude * (1 - 1.5 / 16))
        //        {
        //            qCWarning(KSTARS) << "SBL::fillToMag( " << maglim << " ) failed for trixel " << currentRegion;
        //        }

        t_dynamicLoad += t.restart();

        //        qDebug() << "Drawing SBL for trixel " << currentRegion << ", SBL has "
        //                 <<  m_starBlockList[ currentRegion ]->getBlockCount() << " blocks";

        // REMARK: The following should never carry state, except for const parameters like updateID and maglim
        std::function<void(std::shared_ptr<StarBlock>)> mapFunction = [&updateID, &maglim](std::shared_ptr<StarBlock> myBlock)
        {
            for (StarObject &star : myBlock->contents())
            {
                if (star.updateID != updateID)
                    star.JITupdate();
                if (star.mag() > maglim)
                    break;
            }
        };

        QtConcurrent::blockingMap(m_starBlockList.at(currentRegion)->contents(), mapFunction);

        for (int i = 0; i < m_starBlockList.at(currentRegion)->getBlockCount(); ++i)
        {
            std::shared_ptr<StarBlock> block = m_starBlockList.at(currentRegion)->block(i);
            //            qDebug() << "---> Drawing stars from block " << i << " of trixel " <<
            //                currentRegion << ". SB has " << block->getStarCount() << " stars";
            for (int j = 0; j < block->getStarCount(); j++)
            {
                StarObject *curStar = block->star(j);

                //                qDebug() << "We claim that he's from trixel " << currentRegion
                //<< ", and indexStar says he's from " << m_skyMesh->indexStar( curStar );

                float mag = curStar->mag();

                if (mag > maglim)
                    break;

                if (skyp->drawPointSource(curStar, mag, curStar->spchar()))
                    visibleStarCount++;
            }
        }

        // DEBUG: Uncomment to identify problems with Star Block Factory / preservation of Magnitude Order in the LRU Cache
        //        verifySBLIntegrity();
        t_drawUnnamed += t.restart();
    }
    m_skyMesh->inDraw(false);
#ifdef PROFILE_SINCOS
    trig_calls_here += dms::trig_function_calls;
    trig_redundancy_here += dms::redundant_trig_function_calls;
    cachingdms_bad_uses += CachingDms::cachingdms_bad_uses;
    qDebug() << "Spent " << dms::seconds_in_trig << " seconds doing " << trig_calls_here
             << " trigonometric function calls amounting to an average of "
             << 1000.0 * dms::seconds_in_trig / double(trig_calls_here) << " ms per call";
    qDebug() << "Redundancy of trig calls in this draw: "
             << double(trig_redundancy_here) / double(trig_calls_here) * 100. << "%";
    qDebug() << "CachedDms constructor calls so far: " << CachingDms::cachingdms_constructor_calls;
    qDebug() << "Caching has prevented " << CachingDms::cachingdms_delta << " redundant trig function calls";
    qDebug() << "Bad cache uses in this draw: " << cachingdms_bad_uses;
#endif
#ifdef PROFILE_UPDATECOORDS
    qDebug() << "Spent " << StarObject::updateCoordsCpuTime << " seconds updating " << StarObject::starsUpdated
             << " stars' coordinates (StarObject::updateCoords) for an average of "
             << double(StarObject::updateCoordsCpuTime) / double(StarObject::starsUpdated) * 1.e6 << " us per star.";
#endif

#else
    Q_UNUSED(skyp)
#endif
}

bool DeepStarComponent::openDataFile()
{
    if (starReader.getFileHandle())
        return true;

    starReader.openFile(dataFileName);
    fileOpened = false;
    if (!starReader.getFileHandle())
        qCWarning(KSTARS) << "Failed to open deep star catalog " << dataFileName << ". Disabling it.";
    else if (!starReader.readHeader())
        qCWarning(KSTARS) << "Header read error for deep star catalog " << dataFileName << "!! Disabling it!";
    else
    {
        qint16 faintmag;
        quint8 htm_level;
        int ret = 0;

        ret = fread(&faintmag, 2, 1, starReader.getFileHandle());
        if (starReader.getByteSwap())
            faintmag = bswap_16(faintmag);
        if (starReader.guessRecordSize() == 16)
            m_FaintMagnitude = faintmag / 1000.0;
        else
            m_FaintMagnitude = faintmag / 100.0;
        ret = fread(&htm_level, 1, 1, starReader.getFileHandle());
        qCInfo(KSTARS) << "Processing " << dataFileName << ", HTMesh Level" << htm_level;
        m_skyMesh = SkyMesh::Instance(htm_level);
        if (!m_skyMesh)
        {
            if (!(m_skyMesh = SkyMesh::Create(htm_level)))
            {
                qCWarning(KSTARS) << "Could not create HTMesh of level " << htm_level << " for catalog " << dataFileName
                                  << ". Skipping it.";
                return false;
            }
        }
        ret = fread(&MSpT, 2, 1, starReader.getFileHandle());
        if (starReader.getByteSwap())
            MSpT = bswap_16(MSpT);
        fileOpened = true;
        qCInfo(KSTARS) << "  Sky Mesh Size: " << m_skyMesh->size();
        for (long int i = 0; i < m_skyMesh->size(); i++)
        {
            std::shared_ptr<StarBlockList> sbl(new StarBlockList(i, this));

            if (!sbl.get())
            {
                qCWarning(KSTARS) << "nullptr starBlockList. Expect trouble!";
            }
            m_starBlockList.append(sbl);
        }
        m_zoomMagLimit = 0.06;
    }

    return fileOpened;
}

StarObject *DeepStarComponent::findByHDIndex(int HDnum)
{
    // Currently, we only handle HD catalog indexes
    return m_CatalogNumber.value(HDnum, nullptr); // TODO: Maybe, make this more general.
}

// This uses the main star index for looking up nearby stars but then
// filters out objects with the generic name "star".  We could easily
// build an index for just the named stars which would make this go
// much faster still.  -jbb
//

SkyObject *DeepStarComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    StarObject *oBest = nullptr;

#ifdef KSTARS_LITE
    m_zoomMagLimit = StarComponent::zoomMagnitudeLimit();
#endif
    if (!fileOpened)
        return nullptr;

    m_skyMesh->index(p, maxrad + 1.0, OBJ_NEAREST_BUF);

    MeshIterator region(m_skyMesh, OBJ_NEAREST_BUF);

    while (region.hasNext())
    {
        Trixel currentRegion = region.next();

        // Safety check if the current region is in star block list
        if ((int)currentRegion >= m_starBlockList.size())
            continue;

        for (int i = 0; i < m_starBlockList.at(currentRegion)->getBlockCount(); ++i)
        {
            std::shared_ptr<StarBlock> block = m_starBlockList.at(currentRegion)->block(i);
            for (int j = 0; j < block->getStarCount(); ++j)
            {
#ifdef KSTARS_LITE
                StarObject *star = &(block->star(j)->star);
#else
                StarObject *star = block->star(j);
#endif
                if (!star)
                    continue;
                if (star->mag() > m_zoomMagLimit)
                    continue;

                double r = star->angularDistanceTo(p).Degrees();
                if (r < maxrad)
                {
                    oBest  = star;
                    maxrad = r;
                }
            }
        }
    }

    // TODO: What if we are looking around a point that's not on
    // screen? objectNearest() will need to keep on filling up all
    // trixels around the SkyPoint to find the best match in case it
    // has not been found. Ideally, this should be implemented in a
    // different method and should be called after all other
    // candidates (eg: DeepSkyObject::objectNearest()) have been
    // called.

    return oBest;
}

bool DeepStarComponent::starsInAperture(QList<StarObject *> &list, const SkyPoint &center, float radius, float maglim)
{
    if (maglim < triggerMag)
        return false;

    // For DeepStarComponents, whether we use ra0() and dec0(), or
    // ra() and dec(), should not matter, because the stars are
    // repeated in all trixels that they will pass through, although
    // the factuality of this statement needs to be verified

    // Ensure that we have deprecessed the (RA, Dec) to (RA0, Dec0)
    Q_ASSERT(center.ra0().Degrees() >= 0.0);
    Q_ASSERT(center.dec0().Degrees() <= 90.0);

    m_skyMesh->intersect(center.ra0().Degrees(), center.dec0().Degrees(), radius, (BufNum)OBJ_NEAREST_BUF);

    MeshIterator region(m_skyMesh, OBJ_NEAREST_BUF);

    if (maglim < -28)
        maglim = m_FaintMagnitude;

    while (region.hasNext())
    {
        Trixel currentRegion = region.next();
        // FIXME: Build a better way to iterate over all stars.
        // Ideally, StarBlockList should have such a facility.
        std::shared_ptr<StarBlockList> sbl = m_starBlockList[currentRegion];
        sbl->fillToMag(maglim);
        for (int i = 0; i < sbl->getBlockCount(); ++i)
        {
            std::shared_ptr<StarBlock> block = sbl->block(i);
            for (int j = 0; j < block->getStarCount(); ++j)
            {
#ifdef KSTARS_LITE
                StarObject *star = &(block->star(j)->star);
#else
                StarObject *star = block->star(j);
#endif
                if (star->mag() > maglim)
                    break; // Stars are organized by magnitude, so this should work
                if (star->angularDistanceTo(&center).Degrees() <= radius)
                    list.append(star);
            }
        }
    }

    return true;
}

void DeepStarComponent::byteSwap(DeepStarData *stardata)
{
    stardata->RA   = bswap_32(stardata->RA);
    stardata->Dec  = bswap_32(stardata->Dec);
    stardata->dRA  = bswap_16(stardata->dRA);
    stardata->dDec = bswap_16(stardata->dDec);
    stardata->B    = bswap_16(stardata->B);
    stardata->V    = bswap_16(stardata->V);
}

void DeepStarComponent::byteSwap(StarData *stardata)
{
    stardata->RA       = bswap_32(stardata->RA);
    stardata->Dec      = bswap_32(stardata->Dec);
    stardata->dRA      = bswap_32(stardata->dRA);
    stardata->dDec     = bswap_32(stardata->dDec);
    stardata->parallax = bswap_32(stardata->parallax);
    stardata->HD       = bswap_32(stardata->HD);
    stardata->mag      = bswap_16(stardata->mag);
    stardata->bv_index = bswap_16(stardata->bv_index);
}

bool DeepStarComponent::verifySBLIntegrity()
{
    float faintMag = -5.0;
    bool integrity = true;

    for (Trixel trixel = 0; trixel < m_skyMesh->size(); ++trixel)
    {
        for (int i = 0; i < m_starBlockList[trixel]->getBlockCount(); ++i)
        {
            std::shared_ptr<StarBlock> block = m_starBlockList[trixel]->block(i);

            if (i == 0)
                faintMag = block->getBrightMag();
            // NOTE: Assumes 2 decimal places in magnitude field. TODO: Change if it ever does change
            if (block->getBrightMag() != faintMag && (block->getBrightMag() - faintMag) > 0.5)
            {
                qCWarning(KSTARS) << "Trixel " << trixel << ": ERROR: faintMag of prev block = " << faintMag
                                  << ", brightMag of block #" << i << " = " << block->getBrightMag();
                integrity = false;
            }
            if (i > 1 && (!block->prev))
                qCWarning(KSTARS) << "Trixel " << trixel << ": ERROR: Block" << i << "is unlinked in LRU Cache";
            if (block->prev && block->prev->parent == m_starBlockList[trixel].get() &&
                    block->prev != m_starBlockList[trixel]->block(i - 1))
            {
                qCWarning(KSTARS) << "Trixel " << trixel
                                  << ": ERROR: SBF LRU Cache linked list seems to be broken at before block " << i;
                integrity = false;
            }
            faintMag = block->getFaintMag();
        }
    }
    return integrity;
}
