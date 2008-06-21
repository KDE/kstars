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
 *Represents the stars on the sky map. For optimization reasons the stars are
 *not separate objects and are stored in a list.
 *
 *@author Thomas Kabelmann
 *@version 0.1
 */

#include "listcomponent.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "starblock.h"
#include "skylabel.h"
#include "typedef.h"
#include "highpmstarlist.h"
#include "starobject.h"
#include "binfilehelper.h"
#include "starblockfactory.h"
#include "skymesh.h"

class SkyComponent;
class KStarsData;
class KSFileReader;
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
public:

    StarComponent( SkyComponent* );

    virtual ~StarComponent();

    //This function is empty; we need that so that the JiT update 
    //is the only one beiong used.
    void update( KStarsData *data, KSNumbers *num );

    bool selected();

    void reindex( KSNumbers *num );

    void draw( QPainter& psky );

    /* @short draw all the labels in the prioritized LabelLists and then
     * clear the LabelLists.
     */
    void drawLabels( QPainter& psky );

    void init(KStarsData *data);

    KStarsData *data() { return m_Data; }

    /**@return the current setting of the color mode for stars (0=real colors,
        *1=solid red, 2=solid white or 3=solid black).
        */
    int starColorMode( void ) const;

    /**@short Retrieve the color-intensity value for stars.
        *
        *When using the "realistic colors" mode for stars, stars are rendered as 
        *white circles with a colored border.  The "color intensity" setting modulates
        *the relative thickness of this colored border, so it effectively adjusts
        *the color-saturation level for star images.
        *@return the current setting of the color intensity setting for stars.
        */
    int starColorIntensity( void ) const;

    float faintMagnitude() const { return m_FaintMagnitude; }

    /**
     *@short Read data for stars which will remain static in the memory
     *
     *This method reads data for 'shallow' stars (stars having names, and all 
     *stars down to mag 8) which are stored by default in "shallowstars.dat" into
     *memory. These stars are always kept in memory, as against 'deep' stars
     *which are dynamically loaded into memory when required, depending on region
     *and magnitude limit. Once loading is successful, this method sets the 
     *starsLoaded flag to true
     */

    void loadShallowStarData();

    SkyObject* objectNearest(SkyPoint *p, double &maxrad );

    SkyObject* findStarByGenetiveName( const QString name );

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

    SkyObject* findByName( const QString &name );

    /**
     *@short Prints some useful debug info about memory allocation for stars
     */
    void printDebugInfo();

    /**
     *@short Verifies the integrity of the StarBlockLists
     *
     * This method, useful for debugging, verifies that all SBs in each SBL of
     * the StarComponent are in order of magnitude. It prints out debug info
     * regarding the same.
     *
     *@return true if no errors found, false if an error was found
     */
    bool verifySBLIntegrity();

    // TODO: Find the right place for this method
    static void byteSwap( starData *stardata );

    static StarBlockFactory m_StarBlockFactory;
    static BinFileHelper deepStarReader;

private:
    SkyMesh*       m_skyMesh;
    StarIndex*     m_starIndex;

    KSNumbers      m_reindexNum;
    double         m_reindexInterval;

    int            m_lineNumber[ MAX_LINENUMBER_MAG + 1 ];
    LabelList*     m_labelList[  MAX_LINENUMBER_MAG + 1 ];
    qint64         m_lastFilePos;
    int            m_lastLineNum;
    bool           m_validLineNums;
    bool           m_hideLabels;

    KStarsData*    m_Data;
    float          m_FaintMagnitude;
    bool           starsLoaded;
    float          m_zoomMagLimit;
    float          magLim;
    unsigned long  visibleStarCount;

    KStarsSplash*  m_reloadSplash;
    KStarsSplash*  m_reindexSplash;

    // Time keeping variables
    long unsigned  t_drawNamed;
    long unsigned  t_dynamicLoad;
    long unsigned  t_drawUnnamed;
    long unsigned  t_updateCache;

    QVector<HighPMStarList*> m_highPMStars;
    QHash<QString, SkyObject*> m_genName;


    /**
     *@short adds a label to the lists of labels to be drawn prioritized
     *by magnitude.
     */
    void addLabel( const QPointF& p, StarObject *star );

    void reindexAll( KSNumbers *num );

    /**
     *@short Structure that holds star name information, to be read as-is from the corresponding binary data file
     */
    typedef struct starName {
        char bayerName[8];
        char longName[32];
    } starName;

    QVector< StarBlockList *> m_starBlockList;
    starData stardata;
    starName starname;

    /**
     *@class StarComponent::TrixelIterator
     *@short An "iterator" that iterates over all visible stars in a given trixel
     *
     *This iterator goes through both the StarBlockList and the StarList
     *@author Akarsh Simha
     *@version 0.1
     */
    // DEPRECATED
    class TrixelIterator {
    public:
        /**
         *Constructor
         *
         *@param par    Pointer to the parent StarComponent class
         *@param trixel Trixel for which this Iterator should work
         */
        TrixelIterator( StarComponent *par, Trixel trixel );

        /**
         *Destructor
         */
        ~TrixelIterator();

        /**
         *@short Tells whether a next StarObject is available
         *@return true if a next StarObject is available
         */
        bool hasNext();

        /**
         *@short Returns the next StarObject
         *@return Pointer to the next StarObject
         */
        StarObject *next();

        /**
         *@short Resets the iterator to its initial state
         */
        void reset();

    private:

        StarComponent *parent;

        // TODO: Change the types to fixed-size types that are uniform
        //       across all classes and uses of the variable
        // Indexes used to identify the current "position" of the iterator
        int blockIndex;                // Index of current block in SBL
        int starIndex;                 // Index of current star in SB
        long Index;                    // Overall index of current star
        bool named;                    // Tells us whether a star is named
        Trixel trixel;
    };

    //    friend class TrixelIterator;

};

#endif
