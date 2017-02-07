/***************************************************************************
                          flagcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri 16 Jan 2009
    copyright            : (C) 2009 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FLAGCOMPONENT_H
#define FLAGCOMPONENT_H


//#include <kio/jobclasses.h>

#include "pointlistcomponent.h"
#include "typedef.h"
#include "starblockfactory.h"
#include "skymesh.h"


/** @class FlagComponent
 * @short Represents a flag on the sky map.
 * Each flag is composed by a SkyPoint where coordinates are stored, an 
 * epoch and a label. This class also stores flag images and associates 
 * each flag with an image.
 * When FlagComponent is created, it seeks all file names beginning with
 * "_flag" in the user directory and *considere them as flag images.
 *
 * The file flags.dat stores coordinates, epoch, image name and label of each
 * flags and is read to init FlagComponent
 *
 * @author Jerome SONRIER
 * @version 1.1
 */
class FlagComponent: public QObject, public PointListComponent
{
    Q_OBJECT
public:
    /** @short Constructor. */
    explicit FlagComponent( SkyComposite* );

    /** @short Destructor. */
    virtual ~FlagComponent();

    virtual void draw( SkyPainter *skyp );

    virtual bool selected();

    virtual void update( KSNumbers *num=0 );

    /** @short Add a flag.
     * @param SkyPoint Sky point in epoch coordinates
     * @param epoch Moment for which celestial coordinates are specified
     * @param image Image name
     * @param label Label of the flag
     */
    void add( const SkyPoint & flagPoint, QString epoch, QString image, QString label, QColor labelColor );

    /** @short Remove a flag.
     * @param index Index of the flag to be remove.
     */
    void remove( int index );

    /** @short Update a flag.
      *@param index index of the flag to be updated.
      *@param epoch new flag epoch.
      *@param image new flag image.
      *@param label new flag label.
      *@param labelColor new flag label color.
      */
    void updateFlag(int index, const SkyPoint &flagPoint, QString epoch, QString image, QString label, QColor labelColor );

    /** @short Return image names.
     * @return the list of all image names
     */
    QStringList getNames();

    /** @short Return the numbers of flags.
     * @return the size of m_PointList
     */
    int size();

    /** @short Get epoch.
     * @return the epoch as a string
     * @param index Index of the flag
     */
    QString epoch( int index );

    /** @short Get label.
     * @return the label as a string
     * @param index Index of the flag
     */
    QString label( int index );

    /** @short Get label color.
     * @return the label color
     * @param index Index of the flag
     */
    QColor labelColor( int index );

    /** @short Get image.
     * @return the image associated with the flag
     * @param index Index of the flag
     */
    QImage image( int index );

    /** @short Get image name.
     * @return the name of the image associated with the flag
     * @param index Index of the flag
     */
    QString imageName( int index );

    /** @short Get images.
     * @return all images that can be use
     */
    QList<QImage> imageList();

    /** @short Get image.
     * @param index Index of the image in m_Images
     * @return an image from m_Images
     */
    QImage imageList( int index );

    /**
     * @brief epochCoords return coordinates recorded in original epoch
     * @param index index of the flag
     * @return pair of RA/DEC in original epoch
     */
    QPair<double, double> epochCoords(int index);

    /** @short Get list of flag indexes near specified SkyPoint with radius specified in pixels.
      *@param point central SkyPoint.
      *@param pixelRadius radius in pixels.
      */
    QList<int> getFlagsNearPix( SkyPoint *point, int pixelRadius );

    /** @short Load flags from flags.dat file. */
    void loadFromFile();

    /** @short Save flags to flags.dat file. */
    void saveToFile();

private:
    // Convert from given epoch to J2000. If epoch is already J2000, do nothing
    void toJ2000(SkyPoint *p, QString epoch);

    QStringList                     m_Epoch;        /**< List of epochs                  */
    QList<QPair<double, double>>    m_EpochCoords;  /**< RA/DEC stored in original Epoch */
    QList<int>                      m_FlagImages;   /**< List of image index             */
    QStringList                     m_Labels;       /**< List of label                   */
    QList<QColor>                   m_LabelColors;  /**< List of label colors            */
    QStringList                     m_Names;        /**< List of image names             */
    QList<QImage>                   m_Images;       /**< List of flag images             */
};

#endif
