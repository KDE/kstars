/***************************************************************************
                          ksdssimage.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 05 Jan 2016 00:24:07 CST
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh@kde.org
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/



#ifndef KSDSSIMAGE_H
#define KSDSSIMAGE_H

#include "dms.h"

#include <QString>
#include <QImage>

/**
 * @class KSDssImage
 * @short Provides a class to hold a DSS Image along with its metadata
 * @author Akarsh Simha <akarsh@kde.org>
 */

class KSDssImage {

 public:

    /**
     * @short Constructor
     */
    KSDssImage( const QString &fileName );

    /**
     * @struct KSDssImage::Metadata
     * @short Structure to hold some DSS image metadata
     *
     * @note Some fields in the structure are redundant. The methods
     * that fill this structure must be designed to fill in the
     * redundancies correctly!
     *
     */
    struct Metadata {
        /**
         * @enum KSDssImage::Metadata::Source
         * @short Contains possible sources for digitized sky-survey images
         */
        enum Source { DSS = 0, SDSS = 1, GenericInternetSource = 2 };
        /**
         * @enum KSDssImage::Metadata::FileFormat
         * @short Contains possible file formats for images
         *
         * @note Although DSS website provides us GIF, we may convert
         * to PNG to incorporate metadata, since by default Qt has no
         * write support for GIF. Besides, PNG compresses better.
         *
         */
        enum FileFormat { FITS = 0, GIF = 1, PNG = 2 };

        QString version; // Used for DSS -- Indicates which version of scans to pull
        QString object; // Name / identifier of the object. Added to metadata
        FileFormat format; // File format used.
        Source src; // DSS / SDSS -- source of the image
        dms ra0; // Center RA (J2000.0)
        dms dec0; // Center Dec (J2000.0)
        float height; // height in arcminutes
        float width; // width in arcminutes
        char band; // photometric band (UBVRI...) Use "?" for unknown.
        int gen; // generation for DSS images, data release for SDSS; use -1 for unknown.
        bool valid; // is this data valid?

        Metadata(); // default constructor
        inline bool isValid() const { return valid; } // convenience method
    };

    inline QImage getImage() const { return m_Image; }
    inline KSDssImage::Metadata getMetadata() const { return m_Metadata; }
    inline QString getFileName() const { return m_FileName; }

    // TODO: Implement methods to load and read FITS image data and metadata

 private:
    // TODO: Add support for FITS
    QString m_FileName;
    QImage m_Image;
    Metadata m_Metadata;

};

#endif
