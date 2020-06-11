/***************************************************************************
                          fitssepdetector.h  -  FITS Image
                             -------------------
    begin                : Sun March 29 2020
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

#ifndef FITSSEPDETECTOR_H
#define FITSSEPDETECTOR_H

#include <QObject>

#include "fitsstardetector.h"

class SkyBackground
{
public:
    // Must call initialize() if using the default constructor;
    SkyBackground() {}
    SkyBackground(double m, double sig, double np);
    virtual ~SkyBackground() = default;

    // Mean of the background level (ADUs).
    double mean {0};
    // Standard deviation of the background level.
    double sigma {0};
    // Number of pixels used to estimate the background level.
    int numPixelsInSkyEstimate {0};

    // Given a source with flux spread over numPixels, and a CCD with gain = ADU/#electron)
    // returns an SNR estimate.
    double SNR(double flux, double numPixels, double gain = 0.5);
    void initialize(double mean_, double sigma_, double numPixelsInSkyEstimate_);
private:
    double varSky;
};

class FITSSEPDetector : public FITSStarDetector
{
    Q_OBJECT

public:
    explicit FITSSEPDetector(FITSData *parent): FITSStarDetector(parent) {};

public:
    /** @brief Find sources in the parent FITS data file.
     * @see FITSStarDetector::findSources().
     */
    int findSources(QList<Edge*> &starCenters, QRect const &boundary = QRect()) override;

    /** @brief Find sources in the parent FITS data file as well as background sky information.
     */
    int findSourcesAndBackground(QList<Edge*> &starCenters, QRect const &boundary = QRect(),
                                 SkyBackground *bg = nullptr);

    /** @brief Configure the detection method.
     * @see FITSStarDetector::configure().
     * @note No parameters are currently available for configuration.
     * @todo Provide parameters for detection configuration.
     */
    FITSSEPDetector & configure(const QString &setting, const QVariant &value) override;

protected:
    /** @internal Consolidate a float data buffer from FITS data.
     * @param buffer is the destination float block.
     * @param x, y, w, h define a (x,y)-(x+w,y+h) sub-frame to extract from the FITS data out to block 'buffer'.
     * @param image_data is the FITS data block to extract from.
     */
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h, FITSData const * image_data) const;

  int numStars = 100;
  double fractionRemoved = 0.2;
  int deblendNThresh = 32;
  double deblendMincont = 0.005;
};

#endif // FITSSEPDETECTOR_H
