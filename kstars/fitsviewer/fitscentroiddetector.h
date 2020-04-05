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
     * @note Parameter "initStdDev" defaults to MINIMUM_STDVAR.
     * @note Parameter "minEdgeWidth" defaults to MINIMUM_PIXEL_RANGE.
     * @todo Provide all constants of this class as parameters, and explain their use.
     */
    FITSStarDetector & configure(const QString &setting, const QVariant &value) override;

public:
    /** @group Detection internals
     * @{ */
    static constexpr int MINIMUM_STDVAR { 5 };
    static constexpr int MINIMUM_PIXEL_RANGE { 5 };
    static constexpr int MINIMUM_EDGE_LIMIT { 2 };
    static constexpr int MAX_EDGE_LIMIT { 10000 };
    static constexpr double DIFFUSE_THRESHOLD { 0.15 };
    static constexpr int MINIMUM_ROWS_PER_CENTER { 3 };
    static constexpr int LOW_EDGE_CUTOFF_1  { 50 };
    static constexpr int LOW_EDGE_CUTOFF_2  { 10 };
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

protected:
    int m_initStdDev { MINIMUM_STDVAR };
    int m_minEdgeWidth { MINIMUM_PIXEL_RANGE };
};

#endif // FITSCENTROIDDETECTOR_H
