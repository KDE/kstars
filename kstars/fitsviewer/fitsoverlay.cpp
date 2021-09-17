/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsoverlay.h"

#include "fitsimage.h"

#include <KTemporaryFile>
#include <KIO/CopyJob>
#include <KIO/JobUiDelegate>

FITSOverlay::FITSOverlay()
{
}

void FITSOverlay::addFITSOverlay(const dms &ra, const dms &dec, const QUrl &imageURL)
{
    m_ImageUrl = imageURL;
    this->ra   = ra;
    this->dec  = dec;

    // check URL
    if (!m_ImageUrl.isValid())
        kDebug() << "URL is malformed: " << m_ImageUrl;

    // FIXME: check the logic with temporary files. Races are possible
    {
        KTemporaryFile tempfile;
        tempfile.open();
        file.setFileName(tempfile.fileName());
    } // we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be show

    loadImageFromURL();
}

FITSOverlay::~FITSOverlay()
{
    if (downloadJob)
    {
        // close job quietly, without emitting a result
        downloadJob->kill(KJob::Quietly);
        delete downloadJob;
    }

    qDeleteAll(fList);
}

void FITSOverlay::loadImageFromURL()
{
    QUrl saveURL = QUrl::fromPath(file.fileName());
    if (!saveURL.isValid())
        kDebug() << "tempfile-URL is malformed\n";

    qDebug() << "Starting download job for URL " << m_ImageUrl << endl;

    downloadJob = KIO::copy(m_ImageUrl, saveURL); // starts the download asynchron
    connect(downloadJob, SIGNAL(result(KJob*)), SLOT(downloadReady(KJob*)));
}

void FITSOverlay::downloadReady(KJob *job)
{
    // set downloadJob to 0, but don't delete it - the job will be deleted automatically !!!
    downloadJob = 0;

    if (job->error())
    {
        static_cast<KIO::Job *>(job)->ui()->showErrorMessage();
        return;
    }

    file.close(); // to get the newest information from the file and not any information from opening of the file

    qDebug() << "Download OK , opening image now ..." << endl;
    if (file.exists())
    {
        openImage();
        return;
    }
}

void FITSOverlay::openImage()
{
    FOverlay *newFO = new FOverlay;

    newFO->image_data = new FITSImage(FITS_NORMAL);

    qDebug() << "Reading FITS file ..." << endl;
    bool result = newFO->image_data->loadFITS(file.fileName());

    if (result == false)
    {
        delete (newFO->image_data);
        return;
    }

    qDebug() << "Read successful, creating fits overlay now ..." << endl;

    int image_width, image_height;
    double min, max, bzero, bscale, val;
    float *image_buffer;

    image_width  = newFO->image_data->getWidth();
    image_height = newFO->image_data->getHeight();
    min          = newFO->image_data->getMin();
    max          = newFO->image_data->getMax();

    QImage image(image_width, image_height, QImage::Format_Indexed8);

    image_buffer = newFO->image_data->getImageBuffer();

    bscale = 255. / (max - min);
    bzero  = (-min) * (255. / (max - min));

    image.setNumColors(256);
    for (int i = 0; i < 256; i++)
        image.setColor(i, qRgb(i, i, i));

    /* Fill in pixel values using indexed map, linear scale */
    for (int j = 0; j < image_height; j++)
        for (int i = 0; i < image_width; i++)
        {
            val = image_buffer[j * image_width + i];
            image.setPixel(i, j, ((int)(val * bscale + bzero)));
        }

    newFO->pix.convertFromImage(image);
    newFO->ra         = ra;
    newFO->dec        = dec;
    newFO->pix_width  = image_width;
    newFO->pix_height = image_height;

    qDebug() << "Added a new pixmap FITS!" << endl;

    fList.append(newFO);
}

bool FITSOverlay::contains(const dms &ra, const dms &dec)
{
    return false;
}

#include "fitsoverlay.moc"
