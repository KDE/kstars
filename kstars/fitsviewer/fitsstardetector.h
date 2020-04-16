/***************************************************************************
                          fitsstardetector.h  -  FITS Image
                             -------------------
    begin                : Fri March 27 2020
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

#ifndef FITSSTARDETECTION_H
#define FITSSTARDETECTION_H

#include <QObject>
#include <QHash>
#include <QStandardItem>

#include "fitsdata.h"

class FITSData;

class Edge
{
public:
    virtual ~Edge() = default;
    float x {0};
    float y {0};
    int val {0};
    int scanned {0};
    float width {0};
    float HFR {-1};
    float sum {0};
};

class BahtinovEdge : public Edge
{
public:
    virtual ~BahtinovEdge() = default;
    QVector<QLineF> line;
    QPointF offset;
};

class FITSStarDetector : public QObject
{
    Q_OBJECT

public:
    /** @brief Instantiate a detector for a FITS data file.
     */
    explicit FITSStarDetector(FITSData *parent):
        QObject(reinterpret_cast<QObject*>(parent))
    {};

public:
    /** @brief Find sources in the parent FITS data file.
     * @param starCenters is the list of sources to append to.
     * @param boundary is the rectangle in which to find sources, by default the full frame.
     * @return The number of sources detected by the procedure.
     */
    virtual int findSources(QList<Edge*> &starCenters, QRect const &boundary = QRect()) = 0;

    /** @brief Configure the detection method.
     * @param setting is the name of a detection setting.
     * @param value is the value of the detection setting identified by 'setting'.
     * @return The detector as a chain to call the overriden findSources.
     */
    virtual FITSStarDetector & configure(const QString &setting, const QVariant &value) = 0;

public:
    /** @brief Helper to configure the detection method from a data model.
     * @param settings is the list of key/value pairs for the method to use settings from.
     * @note Data model 'settings' is considered a key/value list, using column 1 text as case-insensitive keys and column 2 data as values.
     * @return The detector as a chain to call the overriden findSources.
     */
    FITSStarDetector & configure(QStandardItemModel const &settings);
};

#endif // FITSSTARDETECTION_H
