/***************************************************************************
                   ksdssdownloader.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 05 Jan 2016 03:22:50 CST
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/



#ifndef KSDSSDOWNLOADER_H
#define KSDSSDOWNLOADER_H

#include "ksdssimage.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>

#include <functional>

class FileDownloader;
class SkyPoint;
class dms;

/**
 * @class KSDssDownloader
 * @short Helps download a DSS image
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 *
 * @note This object is designed to commit suicide (calls
 * QObject::deleteLater() )! Never allocate this using anything but
 * new -- do not allocate it on the stack! This is ideal for its
 * operation, as it deletes itself after downloading.
 */

class KSDssDownloader : public QObject {

    Q_OBJECT

 public:

    /**
     * @short Constructor
     */
    KSDssDownloader( QObject *parent = 0 );

    /**
     * @short Constructor that initiates a "standard" DSS download job, calls the downloadReady slot, and finally self destructs
     * @note Very important that if you create with this constructor,
     * the object will self-destruct. Avoid keeping pointers to it, or
     * things may segfault!
     */
    KSDssDownloader( const SkyPoint * const p, const QString &destFileName, const std::function<void(bool)> &slotDownloadReady, QObject *parent = 0 );

    /**
     * @short Stateful single-download of a supplied URL. Use when the flexibility is required
     * @note Does not self-delete this object. Construct with default constructor, and delete as usual.
     * @param srcUrl source DSS URL to download
     * @param destFileName destination image file (will be of PNG format)
     * @param md DSS image metadata to write into image file
     * @note emits downloadComplete with success state when done
     */
    void startSingleDownload( const QUrl srcUrl, const QString &destFileName, KSDssImage::Metadata md );

    /**
     *@short High-level method to create a URL to obtain a DSS image for a given SkyPoint
     *@note If SkyPoint is a DeepSkyObject, this method automatically
     *decides the image size required to fit the object.
     *@note Moved from namespace KSUtils (--asimha, Jan 5 2016)
     */
    static QString getDSSURL( const SkyPoint * const p, const QString &version = "all", struct KSDssImage::Metadata *md = 0 );

    /**
     *@short High-level method to create a URL to obtain a DSS image for a given SkyPoint
     *@note This method includes an option to set the height, but uses default values for many parameters
     */
    static QString getDSSURL( const SkyPoint * const p, float width, float height = 0, const QString &version = "all", struct KSDssImage::Metadata *md = 0 );

    /**
     *@short Create a URL to obtain a DSS image for a given RA, Dec
     *@param RA The J2000.0 Right Ascension of the point
     *@param Dec The J2000.0 Declination of the point
     *@param width The width of the image in arcminutes
     *@param height The height of the image in arcminutes
     *@param version string describing which version to get
     *@param type The image type, either gif or fits.
     *@param md If a valid pointer is provided, fill with metadata
     *@note This method resets height and width to fall within the range accepted by DSS
     *@note Moved from namespace KSUtils (--asimha, Jan 5 2016)
     *
     *@note Valid versions are: dss1, poss2ukstu_red, poss2ukstu_ir,
     *poss2ukstu_blue, poss1_blue, poss1_red, all, quickv,
     *phase2_gsc2, phase2_gsc1. Of these, dss1 uses POSS1 Red in the
     *north and POSS2/UKSTU Blue in the south. all uses the best of a
     *combined list of all plates.
     *
     */
    static QString getDSSURL( const dms &ra, const dms &dec, float width = 0, float height = 0, const QString & type_ = "gif", const QString & version_ = "all", struct KSDssImage::Metadata *md = 0 );

    /**
     *@short Write image metadata into file
     */
    static bool writeImageWithMetadata( const QString &srcFile, const QString &destFile, const KSDssImage::Metadata &md );

 signals:
     void downloadComplete( bool success );
     void downloadCanceled();

 private slots:
     void downloadAttemptFinished();
     void singleDownloadFinished();
     void downloadError(const QString &errorString);

 private:
     void startDownload( const SkyPoint * const p, const QString &destFileName );
     void initiateSingleDownloadAttempt( QUrl srcUrl );
     bool writeImageFile();

     QStringList m_VersionPreference;
     int m_attempt;
     struct KSDssImage::Metadata m_AttemptData;
     QString m_FileName;
     QTemporaryFile m_TempFile;
     FileDownloader *downloadJob;

};

#endif
