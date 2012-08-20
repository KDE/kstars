/***************************************************************************
                          FITSImage.cpp  -  FITS Image
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
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

#include "fitsimage.h"

#include <math.h>
#include <stdlib.h>

#include <QApplication>
#include <QPaintEvent>
#include <QScrollArea>
#include <QFile>
#include <QCursor>
#include <QProgressDialog>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>

#include <KDebug>
#include <KLocale>
#include <KAction>
#include <KActionCollection>
#include <KStatusBar>
#include <KProgressDialog>
#include <KMessageBox>
#include <KFileDialog>

#include "ksutils.h"

#define ZOOM_DEFAULT	100.0
#define ZOOM_MIN	10
#define ZOOM_MAX	400
#define ZOOM_LOW_INCR	10
#define ZOOM_HIGH_INCR	50

const int MINIMUM_PIXEL_RANGE=5;
const int MINIMUM_ROWS_PER_CENTER=3;
const int MAXIMUM_HOR_SEPARATION=10;
const int MAXIMUM_VER_SEPARATION=2;
const int MINIMUM_STDVAR=5;

//#define FITS_LOG

FITSLabel::FITSLabel(FITSImage *img, QWidget *parent) : QLabel(parent)
{
    image = img;
}

FITSLabel::~FITSLabel() {}

void FITSLabel::mouseMoveEvent(QMouseEvent *e)
{
    double x,y, width, height;
    float *buffer = image->getImageBuffer();

    image->getFITSSize(&width, &height);

    if (buffer == NULL)
        return;

    x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

    emit newStatus(QString("%1 , %2").arg( (int)x ).arg( (int)y ), FITS_POSITION);

    // Range is 0 to dim-1 when accessing array
    x -= 1;
    y -= 1;

    emit newStatus(KGlobal::locale()->formatNumber( buffer[(int) (y * width + x)]), FITS_VALUE);

    setCursor(Qt::CrossCursor);

    e->accept();
}

FITSImage::FITSImage(QWidget * parent) : QScrollArea(parent) , zoomFactor(1.2)
{
    image_frame = new FITSLabel(this);
    image_buffer = NULL;
    displayImage = NULL;
    setBackgroundRole(QPalette::Dark);

    currentZoom = 0.0;
    markStars = false;

    connect(image_frame, SIGNAL(newStatus(QString,FITSBar)), this, SIGNAL(newStatus(QString,FITSBar)));

    image_frame->setMouseTracking(true);

    // Default size
    resize(INITIAL_W, INITIAL_H);
}

FITSImage::~FITSImage()
{
    int status;

    fits_close_file(fptr, &status);

    delete(image_buffer);
    delete(displayImage);

    qDeleteAll(starCenters);

}

bool FITSImage::loadFITS ( const QString &filename )
{

    int status=0, nulval=0, anynull=0;
    long fpixel[2], nelements, naxes[2];
    char error_status[512];

    qDeleteAll(starCenters);
    starCenters.clear();

    QProgressDialog fitsProg(i18n("Please hold while loading FITS file..."), i18n("Cancel"), 0, 100, NULL);
    fitsProg.setWindowTitle(i18n("Loading FITS"));
    //fitsProg.setWindowModality(Qt::WindowModal);

     fitsProg.setValue(30);

    if (fits_open_image(&fptr, filename.toAscii(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        KMessageBox::error(0, i18n("Could not open file %1 (fits_get_img_param). Error %2", filename, QString::fromUtf8(error_status)), i18n("FITS Open"));
        return false;
    }


    if (fitsProg.wasCanceled())
        return false;

    fitsProg.setValue(50);
    //qApp->processEvents(QEventLoop::ExcludeSocketNotifiers);

    if (fits_get_img_param(fptr, 2, &(stats.bitpix), &(stats.ndim), naxes, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        KMessageBox::error(0, i18n("FITS file open error (fits_get_img_param): %1", QString::fromUtf8(error_status)), i18n("FITS Open"));
        return false;
    }

    if (fits_get_img_type(fptr, &data_type, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        KMessageBox::error(0, i18n("FITS file open error (fits_get_img_type): %1", QString::fromUtf8(error_status)), i18n("FITS Open"));
        return false;
    }

    if (fitsProg.wasCanceled())
        return false;

    fitsProg.setValue(60);

    stats.dim[0] = naxes[0];
    stats.dim[1] = naxes[1];


    delete (image_buffer);
    delete (displayImage);
    image_buffer = NULL;
    displayImage = NULL;

    image_buffer = new float[stats.dim[0] * stats.dim[1]];

    if (image_buffer == NULL)
    {
	// Display error message here after freeze
        kDebug() << "Not enough memory for image_buffer";
        return false;
    }

    displayImage = new QImage(stats.dim[0], stats.dim[1], QImage::Format_Indexed8);

    if (displayImage == NULL)
    {
	// Display error message here after freeze
        kDebug() << "Not enough memory for display_image";
        return false;
    }

    if (fitsProg.wasCanceled())
    {
      delete (image_buffer);
      delete (displayImage);
      return false;
    }

    fitsProg.setValue(70);

    displayImage->setNumColors(256);

    for (int i=0; i < 256; i++)
        displayImage->setColor(i, qRgb(i,i,i));

    nelements = stats.dim[0] * stats.dim[1];
    fpixel[0] = 1;
    fpixel[1] = 1;

    if (fits_read_pix(fptr, TFLOAT, fpixel, nelements, &nulval, image_buffer, &anynull, &status))
    {
        fprintf(stderr, "fits_read_pix error\n");
        fits_report_error(stderr, status);
        return false;
    }

    if (fitsProg.wasCanceled())
    {
      delete (image_buffer);
      delete (displayImage);
      return false;
    }

    calculateStats();
    fitsProg.setValue(80);

    findCentroid();

    getHFR();
   
    currentZoom   = 100;
    currentWidth  = stats.dim[0];
    currentHeight = stats.dim[1];

    // Rescale to fits window
    if (rescale(ZOOM_FIT_WINDOW))
        return false;

    if (fitsProg.wasCanceled())
    {
      delete (image_buffer);
      delete (displayImage);
      return false;
    }

    fitsProg.setValue(90);



    if (fitsProg.wasCanceled())
    {
      delete (image_buffer);
      delete (displayImage);
      return false;
    }

    fitsProg.setValue(100);

    setAlignment(Qt::AlignCenter);

    emit newStatus(QString("%1%x%2").arg(currentWidth).arg(currentHeight), FITS_RESOLUTION);



    return true;

}

int FITSImage::saveFITS( const QString &filename )
{
    int status=0;
    long fpixel[2], nelements;
    fitsfile *new_fptr;

    nelements = stats.dim[0] * stats.dim[1];
    fpixel[0] = 1;
    fpixel[1] = 1;


    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, filename.toAscii(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* Copy ALL contents */
    if (fits_copy_file(fptr, new_fptr, 1, 1, 1, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* close current file */
    if (fits_close_file(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    fptr = new_fptr;

    /* Write Data */
    if (fits_write_pix(fptr, TFLOAT, fpixel, nelements, image_buffer, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(stats.min), "Minimum value", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(stats.max), "Maximum value", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    QString history = QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toAscii(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    return status;
}


int FITSImage::calculateMinMax(bool refresh)
{
    int status, nfound=0;
    long npixels;

    status = 0;

    if (refresh == false)
    {
        if (fits_read_key_dbl(fptr, "DATAMIN", &(stats.min), NULL, &status) ==0)
            nfound++;

        if (fits_read_key_dbl(fptr, "DATAMAX", &(stats.max), NULL, &status) == 0)
            nfound++;

        // If we found both keywords, no need to calculate them
        if (nfound == 2)
            return 0;
    }

    npixels  = stats.dim[0] * stats.dim[1];         /* number of pixels in the image */
    stats.min= 1.0E30;
    stats.max= -1.0E30;

    for (int i=0; i < npixels; i++)
    {
        if (image_buffer[i] < stats.min) stats.min = image_buffer[i];
        else if (image_buffer[i] > stats.max) stats.max = image_buffer[i];
    }

    //qDebug() << "DATAMIN: " << stats.min << " - DATAMAX: " << stats.max;
    return 0;
}

int FITSImage::rescale(FITSZoom type)
{
    float val=0;
    double bscale, bzero;

    if (stats.max == stats.min)
    {
        KMessageBox::error(0, i18n("FITS image is saturated and cannot be displayed."), i18n("FITS Open"));
        return -1;
    }


    bscale = 255. / (stats.max - stats.min);
    bzero  = (-stats.min) * (255. / (stats.max - stats.min));

    image_frame->setScaledContents(true);
    currentWidth  = displayImage->width();
    currentHeight = displayImage->height();

    /* Fill in pixel values using indexed map, linear scale */
    for (int j = 0; j < stats.dim[1]; j++)
        for (int i = 0; i < stats.dim[0]; i++)
        {
            val = image_buffer[j * stats.dim[0] + i];
            displayImage->setPixel(i, j, ((int) (val * bscale + bzero)));
        }

    switch (type)
    {
    case ZOOM_FIT_WINDOW:
        if ((displayImage->width() > width() || displayImage->height() > height()))
        {
            // Find the zoom level which will enclose the current FITS in the default window size (640x480)
            currentZoom = floor( (INITIAL_W / currentWidth) * 10.) * 10.;

            /* If width is not the problem, try height */
            if (currentZoom > ZOOM_DEFAULT)
                currentZoom = floor( (INITIAL_H / currentHeight) * 10.) * 10.;

            currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT);
            currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT);


            if (currentZoom <= ZOOM_MIN)
                emit actionUpdated("view_zoom_out", false);

            updateFrame();
        }
        else
        {
            currentZoom   = 100;
            currentWidth  = stats.dim[0];
            currentHeight = stats.dim[1];

            updateFrame();
        }
        break;

    case ZOOM_KEEP_LEVEL:
    {
        currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT);
        currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT);
        updateFrame();

    }
        break;

    default:
        currentZoom   = 100;
        updateFrame();

        break;
    }

    setWidget(image_frame);


   if (type != ZOOM_KEEP_LEVEL)
       emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    return 0;

}

void FITSImage::ZoomIn()
{

    if (currentZoom < ZOOM_DEFAULT)
        currentZoom += ZOOM_LOW_INCR;
    else
        currentZoom += ZOOM_HIGH_INCR;


    emit actionUpdated("view_zoom_out", true);
    if (currentZoom >= ZOOM_MAX)
        emit actionUpdated("view_zoom_in", false);

    currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT);
    currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

}

void FITSImage::ZoomOut()
{

    if (currentZoom <= ZOOM_DEFAULT)
        currentZoom -= ZOOM_LOW_INCR;
    else
        currentZoom -= ZOOM_HIGH_INCR;

    if (currentZoom <= ZOOM_MIN)
        emit actionUpdated("view_zoom_out", false);

    emit actionUpdated("view_zoom_in", true);

    currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT);
    currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);
}

void FITSImage::updateFrame()
{

    QPixmap displayPixmap;

    if (currentZoom != ZOOM_DEFAULT)
            displayPixmap.convertFromImage(displayImage->scaled( (int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            displayPixmap.convertFromImage(*displayImage);


    QPainter painter(&displayPixmap);

    drawOverlay(&painter);

    image_frame->setPixmap(displayPixmap);
    image_frame->resize( (int) currentWidth, (int) currentHeight);
}

void FITSImage::ZoomDefault()
{
    emit actionUpdated("view_zoom_out", true);
    emit actionUpdated("view_zoom_in", true);


    currentZoom   = ZOOM_DEFAULT;
    currentWidth  = stats.dim[0];
    currentHeight = stats.dim[1];

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    update();

}

void FITSImage::calculateStats(bool refresh)
{
    calculateMinMax(refresh);
    // #1 call average, average is used in std deviation
    stats.average = average();
    // #2 call std deviation
    stats.stddev  = stddev();

}

double FITSImage::average()
{
    double sum=0;
    int row=0;
    int width   = stats.dim[0];
    int height  = stats.dim[1];

    if (!image_buffer) return -1;

    for (int i= 0 ; i < height; i++)
    {
        row = (i * width);
        for (int j= 0; j < width; j++)
            sum += image_buffer[row+j];
    }

    return (sum / (width * height ));

}

double FITSImage::stddev()
{

    int row=0;
    double lsum=0;
    int width   = stats.dim[0];
    int height  = stats.dim[1];

    if (!image_buffer) return -1;

    for (int i= 0 ; i < height; i++)
    {
        row = (i * width);
        for (int j= 0; j < width; j++)
        {
            lsum += (image_buffer[row + j] - stats.average) * (image_buffer[row+j] - stats.average);
        }
    }

    return (sqrt(lsum/(width * height - 1)));


}

void FITSImage::setFITSMinMax(double newMin,  double newMax)
{
    stats.min = newMin;
    stats.max = newMax;
}

int FITSImage::getFITSRecord(QString &recordList, int &nkeys)
{
    char *header;
    int status=0;

    if (fits_hdr2str(fptr, 0, NULL, 0, &header, &nkeys, &status))
    {
        fits_report_error(stderr, status);
        free(header);
        return -1;
    }

    recordList = QString(header);

    free(header);

    return 0;
}


/*** Find center of stars and calculate Half Flux Radius */
void FITSImage::findCentroid()
{
    int initStdDev = MINIMUM_STDVAR;
    double threshold=0;
    double avg = 0;
    double sum=0;
    int pixelRadius =0;
    int pixVal=0;

    QList<Edge*> edges;

    while (initStdDev >= 1)
    {
       threshold = stats.stddev* initStdDev;

       #ifdef FITS_LOG
       qDebug() << "The threshold level is " << threshold << endl;
       #endif

    // Detect "edges" that are above threshold
    for (int i=0; i < stats.dim[1]; i++)
    {
        pixelRadius = 0;

        for(int j=0; j < stats.dim[0]; j++)
        {
            pixVal = image_buffer[j+(i*stats.dim[0])] - stats.min;


            // If pixel value > threshold, let's get its weighted average
            if ( pixVal > threshold)
            {
               avg += j * pixVal;
               sum += pixVal;
               pixelRadius++;
            }
            // Value < threshhold but avg exists
            else if (sum > 0)
            {
                // We found a potential centroid edge
                if (pixelRadius >= (MINIMUM_PIXEL_RANGE - (MINIMUM_STDVAR - initStdDev)))
                {
                    int center = ceil(avg/sum);

                    Edge *newEdge = new Edge();

                    newEdge->x          = center;
                    newEdge->y          = i;
                    newEdge->scanned    = 0;
                    newEdge->val        = image_buffer[center+(i*stats.dim[0])] - stats.min;
                    newEdge->width      = pixelRadius;
                    newEdge->HFR        = 0;

                    #ifdef FITS_LOG
                    qDebug() << "# " << edges.count() << " Center at (" << center << "," << i << ") With a value of " << newEdge->val  << " and width of "
                             << pixelRadius << " pixels." <<endl;
                    #endif

                    edges.append(newEdge);

                }

                // Reset
                avg=0;
                sum=0;
                pixelRadius=0;


            }
         }


     }

    #ifdef FITS_LOG
    qDebug() << "Total number of edges found is: " << edges.count() << endl;
    #endif

    if (edges.count() >= MINIMUM_STDVAR)
        break;

      qDeleteAll(edges);
      edges.clear();
      initStdDev--;
    }

    int cen_count=0;
    int cen_x=0;
    int cen_y=0;
    int cen_v=0;
    int rc_index=0;
    int y_counter=0;

    // Now, let's scan the edges and find the maximum centroid vertically
    for (int i=0; i < edges.count(); i++)
    {
        // If edge scanned already, skip
        if (edges[i]->scanned == 1)
        {
            #ifdef FITS_LOG
            qDebug() << "Skipping check for center " << i << " because it was already counted" << endl;
            #endif
            continue;
        }

        #ifdef FITS_LOG
        qDebug() << "Invetigating edge # " << i << " now ..." << endl;
        #endif

        // Get X, Y, and Val of edge
        cen_x = edges[i]->x;
        cen_y = edges[i]->y;
        cen_v = edges[i]->val;

        cen_count=0;
        y_counter=0;

        // Now let's compare to other edges until we hit a maxima
        for (int j=i; j < edges.count();j++)
        {
            if (edges[j]->scanned)
                continue;

            // Permittable margin of error in X among edges
            if (abs(edges[j]->x-cen_x) <= MAXIMUM_HOR_SEPARATION)
            {

                // Permittable margin of error in Y among edges
                if ( abs(edges[j]->y - (cen_y + y_counter++)) <= MAXIMUM_VER_SEPARATION)
                {
                    // If we encounter something big, note it down
                    if (edges[j]->val >= cen_v)
                    {
                        cen_v = edges[j]->val;
                        rc_index = j;
                    }

                    edges[j]->scanned = 1;
                    cen_count++;
                }
            }
        }

        // If centroid count is within acceptable range
        if (cen_count >= (MINIMUM_ROWS_PER_CENTER - (MINIMUM_STDVAR - initStdDev)))
        {
            // We detected a centroid, let's init it
            Edge *rCenter = new Edge();

            rCenter->x = edges[rc_index]->x;
            rCenter->y = edges[rc_index]->y;
            rCenter->width = edges[rc_index]->width;

           #ifdef FITS_LOG
           qDebug() << "Found a real center with number " << rc_index << "with (" << rCenter->x << "," << rCenter->y << ")" << endl;

           qDebug() << "Profile for this center is:" << endl;
           for (int i=edges[rc_index]->width/2; i >= -(edges[rc_index]->width/2) ; i--)
               qDebug() << "#" << i << " , " << image_buffer[rCenter->x-i+(rCenter->y*stats.dim[0])] - stats.min <<  endl;

           #endif

            // Calculate Total Flux From Center, Half Flux, Full Summation
            double TF=0;
            double HF=0;
            double FSum=0;

            // Complete sum along the radius
            //for (int k=0; k < rCenter->width; k++)
            for (int k=rCenter->width/2; k >= -(rCenter->width/2) ; k--)
                FSum += image_buffer[rCenter->x-k+(rCenter->y*stats.dim[0])] - stats.min;

            // Half flux
            HF = FSum / 2.0;

            // Total flux starting from center
            TF = image_buffer[(rCenter->y * stats.dim[0]) + rCenter->x] - stats.min;

            int pixelCounter = 1;

            // Integrate flux along radius axis until we reach half flux
            for (int k=1; k < rCenter->width/2; k++)
            {
                TF += image_buffer[(rCenter->y * stats.dim[0]) + rCenter->x + k] - stats.min;
                TF += image_buffer[(rCenter->y * stats.dim[0]) + rCenter->x - k] - stats.min;

                if (TF >= HF)
                {
                    #ifdef FITS_LOG
                    qDebug() << "Stopping at TF " << TF << " after #" << k << " pixels." << endl;
                    #endif
                    break;
                }

                pixelCounter++;
            }

            // Calculate weighted Half Flux Radius
            rCenter->HFR = pixelCounter * (HF / TF);
            // Store full flux
            rCenter->val = FSum;

            #ifdef FITS_LOG
            qDebug() << "HFR for this center is " << rCenter->HFR << " pixels and the total flux is " << FSum << endl;
            #endif
             starCenters.append(rCenter);
        }

    }

    // Release memory
    qDeleteAll(edges);
}

void FITSImage::drawOverlay(QPainter *painter)
{
    if (markStars)
        drawStarCentroid(painter);

}

void FITSImage::drawStarCentroid(QPainter *painter)
{
    painter->setPen(QPen(Qt::red, 2));

    for (int i=0; i < starCenters.count() ; i++)
      painter->drawText(starCenters[i]->x * (currentZoom / ZOOM_DEFAULT) -3, starCenters[i]->y * (currentZoom / ZOOM_DEFAULT)+3 , "+");
}

double FITSImage::getHFR()
{
    double FSum=0;

    double avgHFR=0;

    // Weighted average HFR
    for (int i=0; i < starCenters.count() ; i++)
    {
        avgHFR += starCenters[i]->val * starCenters[i]->HFR;
        FSum   += starCenters[i]->val;
    }

    if (FSum != 0)
    {
        //qDebug() << "Average HFR is " << avgHFR / FSum << endl;
        return (avgHFR / FSum);
    }
    else
        return -1;
}

#include "fitsimage.moc"
