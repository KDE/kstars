/***************************************************************************
                          starcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/14/08
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

#ifndef STARCOMPONENT_H
#define STARCOMPONENT_H

/**
 *@class StarComponent 
 *
 *@short Represents the stars on the sky map. For optimization reasons the
 *stars are not separate objects and are stored in a list. 
 *
 *The StarComponent class manages all stars drawn in KStars. While it
 *handles all stars having names using its own member methods, it
 *shunts the responsibility of unnamed stars to the class
 *'DeepStarComponent', objects of which it maintains.
 *
 *@author Thomas Kabelmann
 *@author Akarsh Simha
 *@version 1.0
 */

#include "listcomponent.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "starblock.h"
#include "skylabel.h"
#include "typedef.h"
#include "highpmstarlist.h"
#include "skyobjects/starobject.h"
#include "binfilehelper.h"
#include "starblockfactory.h"
#include "skymesh.h"

class SkyMesh;
class StarObject;
class SkyLabeler;
class KStarsSplash;
class BinFileHelper;
class StarBlockFactory;
class MeshIterator;

#define MAX_LINENUMBER_MAG 90

class StarComponent: public ListComponent
{
protected:

    StarComponent( SkyComposite* );

public:

    virtual ~StarComponent();

    // TODO: Desingletonize StarComponent
    /** @short Create an instance of StarComponent */
    static StarComponent *Create( SkyComposite* );

    /** @return the instance of StarComponent if already created, NULL otherwise */
    static StarComponent *Instance() { return pinstance; }

    //This function is empty; we need that so that the JiT update 
    //is the only one beiong used.
    virtual void update( KSNumbers *num );

    bool selected();

    void draw( SkyPainter *skyp );

    /** @short draw all the labels in the prioritized LabelLists and then
     * clear the LabelLists. */
    void drawLabels();

    static float zoomMagnitudeLimit();

    virtual SkyObject* objectNearest(SkyPoint *p, double &maxrad );

    virtual SkyObject* findStarByGenetiveName( const QString name );

    /**
     *@short Find stars by name (including genetive name)
     *
     * Overrides ListComponent::findByName() to include genetive names of stars
     * as well.
     *
     *@param name  Name to search for. Could be trivial name or genetive name
     *@return  Pointer to the star with the given name as a SkyObject, NULL if
     *         no match was found
     */
    virtual SkyObject* findByName( const QString &name );

    /**
     * @short Searches the region(s) and appends the SkyObjects found to the list of sky objects
     *
     * Look for a SkyObject that is in one of the regions 
     * If found, then append to the list of sky objects
     * @p list list of SkyObject to which matching list has to be appended to
     * @p region defines the regions in which the search for SkyObject should be done within
     * @return void
     */
    virtual void objectsInArea( QList<SkyObject*>& list, const SkyRegion& region );

    /**
     *@short Find stars by HD catalog index
     *@param HDnum HD Catalog Number of the star to find
     *@return If the star is a static star, a pointer to the star will be returned
     *        If it is a dynamic star, a fake copy will be created that survives till
     *        the next findByHDIndex() call. If no match was found, returns NULL.
     */
    StarObject* findByHDIndex( int HDnum );

    /**
     *@short Add to the given list, the stars from this component,
     * that lie within the specified circular aperture, and that are
     * brighter than the limiting magnitude specified.
     *@p center The center point of the aperture
     *@p radius The radius around the center point that defines the
     * aperture
     *@p maglim Optional parameter indicating the limiting magnitude. 
     * If magnitude limit is numerically < -28, the limiting magnitude
     * is assumed to be the limiting magnitude of the catalog (i.e. no
     * magnitude limit) 
     *@p list The list to operate on
     */
    void starsInAperture( QList<StarObject*> &list, const SkyPoint &center, float radius, float maglim=-29 );


    // TODO: Make byteSwap a template method and put it in byteorder.h
    // It should ideally handle 32-bit, 16-bit fields and starData and
    // deepStarData fields
    static void byteSwap( starData *stardata );

private:
    /** @short Read data for stars which will remain static in the memory
     *
     * This method reads data for named stars (stars having names,
     * which are stored by default in "namedstars.dat") into
     * memory. These stars are always kept in memory, as against 'deep'
     * stars which are mostly loaded dynamically (KStars treats all
     * unnamed stars as 'deep' stars) into memory when required,
     * depending on region and magnitude limit. Once loading is
     * successful, this method sets the starsLoaded flag to true
     */
    bool loadStaticData();

    /** @return the magnitude of the faintest star */
    float faintMagnitude() const;

    void reindex( KSNumbers *num );

    
    SkyMesh*       m_skyMesh;
    StarIndex*     m_starIndex;

    KSNumbers      m_reindexNum;
    double         m_reindexInterval;

    LabelList*     m_labelList[  MAX_LINENUMBER_MAG + 1 ];
    bool           m_hideLabels;

    float          m_zoomMagLimit;

    float          m_FaintMagnitude; // Limiting magnitude of the catalog currently loaded
    bool           starsLoaded;
    float          magLim;           // Current limiting magnitude for visible stars

    StarObject     m_starObject;
    StarObject     *focusStar;       // This object is always drawn

    KStarsSplash*  m_reindexSplash;

    StarBlockFactory *m_StarBlockFactory;

    QVector<HighPMStarList*> m_highPMStars;
    QHash<QString, SkyObject*> m_genName;
    QHash<int, StarObject*> m_HDHash;
    QVector<DeepStarComponent*> m_DeepStarComponents;

    /**
     *@short adds a label to the lists of labels to be drawn prioritized
     *by magnitude.
     */
    void addLabel( const QPointF& p, StarObject *star );

    void reindexAll( KSNumbers *num );

    /**
     *@short load available deep star catalogs
     */
    int loadDeepStarCatalogs();

    bool addDeepStarCatalogIfExists( const QString &fileName, float trigMag, bool staticstars=false );

    /**
     *@short Structure that holds star name information, to be read as-is from the corresponding binary data file
     */
    typedef struct starName {
        char bayerName[8];
        char longName[32];
    } starName;

    starData stardata;
    starName starname;

    static StarComponent *pinstance;

};

#endif
