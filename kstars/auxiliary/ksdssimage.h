/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"

#include <QImage>
#include <QString>

/**
 * @class KSDssImage
 * @short Provides a class to hold a DSS Image along with its metadata
 *
 * @author Akarsh Simha <akarsh@kde.org>
 */
class KSDssImage
{
  public:
    /** @short Constructor */
    explicit KSDssImage(const QString &fileName);

    /**
     * @struct KSDssImage::Metadata
     * @short Structure to hold some DSS image metadata
     *
     * @note Some fields in the structure are redundant. The methods
     * that fill this structure must be designed to fill in the
     * redundancies correctly!
     *
     */
    struct Metadata
    {
        /**
         * @enum Source
         * @short Contains possible sources for digitized sky-survey images
         */
        enum Source
        {
            DSS                   = 0,
            SDSS                  = 1,
            GenericInternetSource = 2
        };

        /**
         * @enum FileFormat
         * @short Contains possible file formats for images
         *
         * @note Although DSS website provides us GIF, we may convert
         * to PNG to incorporate metadata, since by default Qt has no
         * write support for GIF. Besides, PNG compresses better.
         *
         */
        enum FileFormat
        {
            FITS = 0,
            GIF  = 1,
            PNG  = 2
        };

        /// Used for DSS -- Indicates which version of scans to pull
        QString version;
        /// Name / identifier of the object. Added to metadata
        QString object;
        /// File format used.
        FileFormat format { FITS };
        /// DSS / SDSS -- source of the image
        Source src { DSS };
        /// Center RA (J2000.0)
        dms ra0;
        /// Center Dec (J2000.0)
        dms dec0;
        /// Height in arcminutes
        float height { 0 };
        /// Width in arcminutes
        float width { 0 };
        /// Photometric band (UBVRI...) Use "?" for unknown.
        char band { '?' };
        /// Generation for DSS images, data release for SDSS; use -1 for unknown.
        int gen { -1 };
        /// Are these data valid?
        bool valid { false };

        inline bool isValid() const
        {
            return valid; // convenience method
        }
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
