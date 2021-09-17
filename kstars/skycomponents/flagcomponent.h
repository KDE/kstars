/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "pointlistcomponent.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QStringList>

class SkyPainter;

/**
 * @class FlagComponent
 * @short Represents a flag on the sky map.
 * Each flag is composed by a SkyPoint where coordinates are stored, an
 * epoch and a label. This class also stores flag images and associates
 * each flag with an image.
 * When FlagComponent is created, it seeks all file names beginning with
 * "_flag" in the user directory and *consider* them as flag images.
 *
 * The file flags.dat stores coordinates, epoch, image name and label of each
 * flags and is read to init FlagComponent
 *
 * @author Jerome SONRIER
 * @version 1.1
 */
class FlagComponent : public QObject, public PointListComponent
{
    Q_OBJECT

  public:
    /** @short Constructor. */
    explicit FlagComponent(SkyComposite *);

    virtual ~FlagComponent() override = default;

    void draw(SkyPainter *skyp) override;

    bool selected() override;

    void update(KSNumbers *num = nullptr) override;

    /**
     * @short Add a flag.
     * @param flagPoint Sky point in epoch coordinates
     * @param epoch Moment for which celestial coordinates are specified
     * @param image Image name
     * @param label Label of the flag
     * @param labelColor Color of the label
     */
    void add(const SkyPoint &flagPoint, QString epoch, QString image, QString label, QColor labelColor);

    /**
     * @short Remove a flag.
     * @param index Index of the flag to be remove.
     */
    void remove(int index);

    /**
     * @short Update a flag.
     * @param index index of the flag to be updated.
     * @param flagPoint point of the flag.
     * @param epoch new flag epoch.
     * @param image new flag image.
     * @param label new flag label.
     * @param labelColor new flag label color.
     */
    void updateFlag(int index, const SkyPoint &flagPoint, QString epoch, QString image, QString label,
                    QColor labelColor);

    /**
     * @short Return image names.
     * @return the list of all image names
     */
    QStringList getNames();

    /**
     * @short Return the numbers of flags.
     * @return the size of m_PointList
     */
    int size();

    /**
     * @short Get epoch.
     * @return the epoch as a string
     * @param index Index of the flag
     */
    QString epoch(int index);

    /**
     * @short Get label.
     * @return the label as a string
     * @param index Index of the flag
     */
    QString label(int index);

    /**
     * @short Get label color.
     * @return the label color
     * @param index Index of the flag
     */
    QColor labelColor(int index);

    /**
     * @short Get image.
     * @return the image associated with the flag
     * @param index Index of the flag
     */
    QImage image(int index);

    /**
     * @short Get image name.
     * @return the name of the image associated with the flag
     * @param index Index of the flag
     */
    QString imageName(int index);

    /**
     * @short Get images.
     * @return all images that can be use
     */
    QList<QImage> imageList();

    /**
     * @short Get image.
     * @param index Index of the image in m_Images
     * @return an image from m_Images
     */
    QImage imageList(int index);

    /**
     * @brief epochCoords return coordinates recorded in original epoch
     * @param index index of the flag
     * @return pair of RA/DEC in original epoch
     */
    QPair<double, double> epochCoords(int index);

    /**
     * @short Get list of flag indexes near specified SkyPoint with radius specified in pixels.
     * @param point central SkyPoint.
     * @param pixelRadius radius in pixels.
     */
    QList<int> getFlagsNearPix(SkyPoint *point, int pixelRadius);

    /** @short Load flags from flags.dat file. */
    void loadFromFile();

    /** @short Save flags to flags.dat file. */
    void saveToFile();

  private:
    // Convert from given epoch to J2000. If epoch is already J2000, do nothing
    void toJ2000(SkyPoint *p, QString epoch);

    /// List of epochs
    QStringList m_Epoch;
    /// RA/DEC stored in original epoch
    QList<QPair<double, double>> m_EpochCoords;
    /// List of image index
    QList<int> m_FlagImages;
    /// List of label
    QStringList m_Labels;
    /// List of label colors
    QList<QColor> m_LabelColors;
    /// List of image names
    QStringList m_Names;
    /// List of flag images
    QList<QImage> m_Images;
};
