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

/**@class StarComponent
*Represents the stars on the sky map. For optimization reasons the stars are
*not separate objects and are stored in a list.

*@author Thomas Kabelmann
*@version 0.1
*/

#include "listcomponent.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"

#include "skylabel.h"
#include "typedef.h"
#include "highpmstarlist.h"

class SkyComponent;
class KStarsData;
class KSFileReader;
class SkyMesh;
class StarObject;
class SkyLabeler;
class KStarsSplash;

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

    void readData( float newMagnitude );

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

    /* @short usually does nothing.  If we are drawing faint stars and if
    	* Options:::magLimitDrawStar() is greater than m_faaintMagnitude then
    	* the first time we are called we just pop up a splash screen. Then
    	* the second time we are called we actually re-read the data file and
    	* finally erase the pop up.
    	*/
    void rereadData();

    /* @short reads in the small starlnum.idx file that contains the line
    	* numbers from the stars.dat file that correspond to rough 90
    	* different magnitudes.  This allows us to estimate the number of
    	* lines that need to get read when partially reading stars.dat.
    	*/
    void readLineNumbers();

    /* @short returns an estimate of the stars.dat line number for a given
    	* star magnitude.
    	*/
    int lineNumber( float mag );


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
    float          m_zoomMagLimit;

    KStarsSplash*  m_reloadSplash;
    KStarsSplash*  m_reindexSplash;

    QVector<HighPMStarList*> m_highPMStars;
    QHash<QString, SkyObject*> m_genName;

    /** 
     *@short adds a label to the lists of labels to be drawn prioritized
     *by magnitude.
     */
    void addLabel( const QPointF& p, StarObject *star );

    void reindexAll( KSNumbers *num );

    /**
    	*Parse a line from a stars data file, construct a StarObject from the data,
    	*and add it to the StarComponent.
    	*
    	*Each line is parsed according to the column
    	*position in the line:
    	*@li 0-1      RA hours [int]
    	*@li 2-3      RA minutes [int]
    	*@li 4-8      RA seconds [float]
    	*@li 10       Dec sign [char; '+' or '-']
    	*@li 11-12    Dec degrees [int]
    	*@li 13-14    Dec minutes [int]
    	*@li 15-18    Dec seconds [float]
    	*@li 20-28    dRA/dt (milli-arcsec/yr) [float]
    	*@li 29-37    dDec/dt (milli-arcsec/yr) [float]
    	*@li 38-44    Parallax (milli-arcsec) [float]
    	*@li 46-50    Magnitude [float]
    	*@li 51-55    B-V color index [float]
    	*@li 56-57    Spectral type [string]
    	*@li 59       Multiplicity flag (1=true, 0=false) [int]
    	*@li 61-64    Variability range of brightness (magnitudes; bank if not variable) [float]
    	*@li 66-71    Variability period (days; blank if not variable) [float]
    	*@li 72-END   Name(s) [string].  This field may be blank.  The string is the common
    	*             name for the star (e.g., "Betelgeuse").  If there is a colon, then
    	*             everything after the colon is the genetive name for the star (e.g.,
    	*             "alpha Orionis").
    	*
    	*@param line pointer to the line of data to be processed as a StarObject
    	*/
    StarObject* processStar( const QString &line );

};

#endif
