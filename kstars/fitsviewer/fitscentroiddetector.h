/***************************************************************************
                          fitscentroiddetector.h  -  FITS Image
                             -------------------
    begin                : Sat March 28 2020
    copyright            : (C) 2004 by Jasem Mutlaq, (C) 2020 by Eric Dejouhanet
    email                : eric.dejouhanet@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

#ifndef FITSCENTROIDDETECTOR_H
#define FITSCENTROIDDETECTOR_H

#include <QObject>
#include "fitsstardetector.h"

class FITSCentroidDetector: public FITSStarDetector
{
    Q_OBJECT

public:
    explicit FITSCentroidDetector(FITSData *parent): FITSStarDetector(parent) {};

public:
    /** @brief Find sources in the parent FITS data file.
     * @see FITSStarDetector::findSources().
     */
    int findSources(QList<Edge*> &starCenters, QRect const &boundary = QRect()) override;

    /** @brief Configure the detection method.
     * @see FITSStarDetector::configure().
     * @see Detection parameters.
     */
    FITSStarDetector & configure(const QString &setting, const QVariant &value) override;

protected:
    /** @group Detection parameters. Use the names as strings for FITSStarDetector::configure().
     * @{ */
    /** @brief Initial variation, decreasing as search progresses. Configurable. */
    int MINIMUM_STDVAR { 5 };
    /** @brief Initial source width, decreasing as search progresses. Configurable. */
    int MINIMUM_PIXEL_RANGE { 5 };
    /** @brief Custom image contrast index from the frame histogram. Configurable. */
    double JMINDEX { 100 };
    /** @brief Initial source count over which search stops. */
    int MINIMUM_EDGE_LIMIT { 2 };
    /** @brief Maximum source count over which search aborts. */
    int MAX_EDGE_LIMIT { 10000 };
    /** @brief Minimum value of JMINDEX under which the custom image contrast index from the histogram is used to redefine edge width and count. */
    double DIFFUSE_THRESHOLD { 0.15 };
    /** @brief */
    int MINIMUM_ROWS_PER_CENTER { 3 };
    /** @brief */
    int LOW_EDGE_CUTOFF_1  { 50 };
    /** @brief */
    int LOW_EDGE_CUTOFF_2  { 10 };
    /** @} */

protected:
    /** @internal Find sources in the parent FITS data file, dependent of the pixel depth.
     * @see FITSGradientDetector::findSources.
     */
    template <typename T>
    int findSources(QList<Edge*> &starCenters, const QRect &boundary);

    /** @internal Check whether two sources overlap.
     * @param s1, s2 are the two sources to check collision on.
     * @return true if the sources collide, else false.
     */
    bool checkCollision(Edge * s1, Edge * s2) const;
};

#endif // FITSCENTROIDDETECTOR_H
