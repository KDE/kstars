/***************************************************************************
                          FITSView.cpp  -  FITS Image
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

#include <config-kstars.h>
#include "fitsview.h"
#include "kspopupmenu.h"
#include "skymap.h"

#include <cmath>
#include <cstdlib>

#include <QApplication>
#include <QPaintEvent>
#include <QtConcurrent>
#include <QScrollArea>
#include <QFile>
#include <QCursor>
#include <QToolTip>
#include <QProgressDialog>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QDebug>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>

#include <QWheelEvent>
#include <QEvent>
#include <QGestureEvent>
#include <QPinchGesture>

#include <QMenu>

#include <KActionCollection>
#include <KMessageBox>
#include <KLocalizedString>

#include "kstarsdata.h"
#include "ksutils.h"
#include "Options.h"

#ifdef HAVE_INDI
#include "basedevice.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/driverinfo.h"
#endif

#define BASE_OFFSET     50
#define ZOOM_DEFAULT	100.0
#define ZOOM_MIN        10
#define ZOOM_MAX        400
#define ZOOM_LOW_INCR	10
#define ZOOM_HIGH_INCR	50

//#define FITS_DEBUG

FITSLabel::FITSLabel(FITSView *img, QWidget *parent) : QLabel(parent)
{
    image = img;

}

void FITSLabel::setSize(double w, double h)
{
    width  = w;
    height = h;
    size   = w*h;
}

FITSLabel::~FITSLabel() {}

/**
This method looks at what mouse mode is currently selected and updates the cursor to match.
 */

void FITSView::updateMouseCursor(){
    if(mouseMode==dragMouse){
        if(horizontalScrollBar()->maximum()>0||verticalScrollBar()->maximum()>0){
            if(!image_frame->getMouseButtonDown())
                viewport()->setCursor(Qt::PointingHandCursor);
            else
                viewport()->setCursor(Qt::ClosedHandCursor);
        }
        else
            viewport()->setCursor(Qt::CrossCursor);
    }
    if(mouseMode==selectMouse){
        viewport()->setCursor(Qt::CrossCursor);
    }
    if(mouseMode==scopeMouse){
        QPixmap scope_pix=QPixmap(":/icons/center_telescope.svg").scaled(32,32,Qt::KeepAspectRatio,Qt::FastTransformation);
        viewport()->setCursor(QCursor(scope_pix,10,10));
    }
}

/**
This is how the mouse mode gets set.
The default for a FITSView in a FITSViewer should be the dragMouse
The default for a FITSView in the Focus or Align module should be the selectMouse
The different defaults are accomplished by putting making the dactual default mouseMode
the selectMouse, but when a FITSViewer loads an image, it immediately makes it the dragMouse.
 */

void FITSView::setMouseMode(int mode){
    if(mode>-1&&mode<3){
        mouseMode=mode;
        updateMouseCursor();
    }
}

bool FITSLabel::getMouseButtonDown(){
    return mouseButtonDown;
}

int FITSView::getMouseMode(){
    return mouseMode;
}

/**
This method was added to make the panning function work.
If the mouse button is released, it resets mouseButtonDown variable and the mouse cursor.
 */
void FITSLabel::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    if(image->getMouseMode()==FITSView::dragMouse){
        mouseButtonDown=false;
        image->updateMouseCursor();
    }
}
/**
I added some things to the top of this method to allow panning and Scope slewing to function.
If you are in the dragMouse mode and the mousebutton is pressed, The method checks the difference
between the location of the last point stored and the current event point to see how the mouse has moved.
Then it moves the scrollbars and thus the image to the right location.
Then it stores the current point so next time it can do it again.
 */
void FITSLabel::mouseMoveEvent(QMouseEvent *e)
{
    float scale=(image->getCurrentZoom() / ZOOM_DEFAULT);

    if(image->getMouseMode()==FITSView::dragMouse&&mouseButtonDown){
        QPoint newPoint=e->globalPos();
        int dx=newPoint.x()-lastMousePoint.x();
        int dy=newPoint.y()-lastMousePoint.y();
        image->horizontalScrollBar()->setValue(image->horizontalScrollBar()->value()-dx);
        image->verticalScrollBar()->setValue(image->verticalScrollBar()->value()-dy);

        lastMousePoint=newPoint;
    }

    double x,y;
    FITSData *image_data = image->getImageData();

    uint8_t *buffer = image_data->getImageBuffer();

    if (buffer == NULL)
        return;

    x = round(e->x() / scale);
    y = round(e->y() / scale);

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

    emit newStatus(QString("X:%1 Y:%2").arg( (int)x ).arg( (int)y ), FITS_POSITION);

    // Range is 0 to dim-1 when accessing array
    x -= 1;
    y -= 1;

    QString stringValue;

    switch (image_data->getDataType())
    {
        case TBYTE:
            stringValue = QLocale().toString(buffer[(int) (y * width + x)]);
            break;

        case TSHORT:
            stringValue = QLocale().toString( (reinterpret_cast<int16_t*>(buffer)) [(int) (y * width + x)]);
            break;

        case TUSHORT:
            stringValue = QLocale().toString( (reinterpret_cast<uint16_t*>(buffer)) [(int) (y * width + x)]);
            break;

        case TLONG:
            stringValue = QLocale().toString( (reinterpret_cast<int32_t*>(buffer)) [(int) (y * width + x)]);
            break;

        case TULONG:
            stringValue = QLocale().toString( (reinterpret_cast<uint32_t*>(buffer)) [(int) (y * width + x)]);
            break;

        case TFLOAT:
            stringValue = QLocale().toString( (reinterpret_cast<float*>(buffer)) [(int) (y * width + x)], 'f', 5);
            break;

        case TLONGLONG:
            stringValue = QLocale().toString(static_cast<int>((reinterpret_cast<int64_t*>(buffer)) [(int) (y * width + x)]));
            break;

        case TDOUBLE:
            stringValue = QLocale().toString( (reinterpret_cast<float*>(buffer)) [(int) (y * width + x)], 'f', 5);

        break;

        default:
        break;
    }

    emit newStatus(stringValue, FITS_VALUE);

    if (image_data->hasWCS()&&image->getMouseMode()!=FITSView::selectMouse)
    {
        int index = x + y * width;

        wcs_point * wcs_coord = image_data->getWCSCoord();

        if (wcs_coord)
        {
            if (index > size)
                return;

            ra.setD(wcs_coord[index].ra);
            dec.setD(wcs_coord[index].dec);

            emit newStatus(QString("%1 , %2").arg( ra.toHMSString()).arg(dec.toDMSString()), FITS_WCS);
        }

        bool objFound=false;
        foreach(FITSSkyObject *listObject, image_data->objList){
            if((std::abs(listObject->x()-x)<5/scale)&&(std::abs(listObject->y()-y)<5/scale)){
                QToolTip::showText(e->globalPos(), listObject->skyObject()->name() +"\n"+listObject->skyObject()->longname() , this);
                objFound=true;
                break;
            }
        }
        if(!objFound)
            QToolTip::hideText();
    }

    //setCursor(Qt::CrossCursor);

    e->accept();
}

/**
I added some things to the top of this method to allow panning and Scope slewing to function.
If in dragMouse mode, the Panning function works by storing the cursor position when the mouse was pressed and setting
the mouseButtonDown variable to true.
If in ScopeMouse mode and the mouse is clicked, if there is WCS data and a scope is available, the method will verify that you actually
do want to slew to the WCS coordinates associated with the click location.  If so, it calls the centerTelescope function.
 */

void FITSLabel::mousePressEvent(QMouseEvent *e)
{
    float scale=(image->getCurrentZoom() / ZOOM_DEFAULT);

    if(image->getMouseMode()==FITSView::dragMouse)
    {
        mouseButtonDown=true;
        lastMousePoint=e->globalPos();
        image->updateMouseCursor();
    } else if(image->getMouseMode()==FITSView::scopeMouse)
    {
#ifdef HAVE_INDI
        FITSData *image_data = image->getImageData();
        if (image_data->hasWCS())
        {

            wcs_point * wcs_coord = image_data->getWCSCoord();
            double x,y;
            x = round(e->x() / scale);
            y = round(e->y() / scale);

            x = KSUtils::clamp(x, 1.0, width);
            y = KSUtils::clamp(y, 1.0, height);
            int index = x + y * width;
            if(KMessageBox::Continue==KMessageBox::warningContinueCancel(NULL, "Slewing to Coordinates: \nRA: " + dms(wcs_coord[index].ra).toHMSString() + "\nDec: " + dms(wcs_coord[index].dec).toDMSString(),
                                 i18n("Continue Slew"),  KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "continue_slew_warning")){
                centerTelescope(wcs_coord[index].ra/15.0, wcs_coord[index].dec);
            }
        }
#endif
    }

    double x,y;

    x = round(e->x() / scale);
    y = round(e->y() / scale);

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

#ifdef HAVE_INDI
    FITSData *image_data = image->getImageData();

    if (e->buttons() & Qt::RightButton&&image->getMouseMode()!=FITSView::selectMouse)
    {
        mouseReleaseEvent(e);
        if (image_data->hasWCS())
        {
            foreach(FITSSkyObject *listObject, image_data->objList){
                if((std::abs(listObject->x()-x)<5/scale)&&(std::abs(listObject->y()-y)<5/scale)){
                    SkyObject *object=listObject->skyObject();
                    KSPopupMenu *pmenu;
                    pmenu=new KSPopupMenu();
                    object->initPopupMenu(pmenu);
                    QList<QAction *> actions= pmenu->actions();
                    foreach(QAction *action,actions){
                        if(action->text().left(7)=="Starhop")
                            pmenu->removeAction(action);
                        if(action->text().left(7)=="Angular")
                            pmenu->removeAction(action);
                        if(action->text().left(8)=="Add flag")
                            pmenu->removeAction(action);
                        if(action->text().left(12)=="Attach Label")
                            pmenu->removeAction(action);
                    }
                    pmenu->popup(e->globalPos());
                    KStars::Instance()->map()->setClickedObject(object);
                    break;
                }
            }
        }

        if (fabs(image->markerCrosshair.x()-x) <= 15 && fabs(image->markerCrosshair.y()-y) <= 15)
            emit markerSelected(0, 0);
    }
#endif


    emit pointSelected(x, y);

    double HFR = image->getImageData()->getHFR(x, y);

    if (HFR > 0)
        QToolTip::showText(e->globalPos(), i18nc("Half Flux Radius", "HFR: %1", QString::number(HFR, 'g' , 3)), this);

}

void FITSLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
    double x,y;

    x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

    emit markerSelected(x, y);

    return;
}

/**
This method just verifies if INDI is online, a telescope present, and is connected
 */

bool FITSView::isTelescopeActive(){
#ifdef HAVE_INDI
    if (INDIListener::Instance()->size() == 0)
    {
        return false;
    }

    foreach(ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == NULL)
            continue;

        return bd->isConnected();
    }
    return false;
#else
    return false;
#endif
}

void FITSLabel::centerTelescope(double raJ2000, double decJ2000)
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
        return;
    }

    foreach(ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == NULL)
            continue;

        if (bd->isConnected() == false)
        {
            KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }

        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        SkyObject selectedObject;

        selectedObject.setRA0(raJ2000);
        selectedObject.setDec0(decJ2000);

        selectedObject.apparentCoord(J2000, KStarsData::Instance()->ut().djd());

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS, &selectedObject);

        return;
    }

    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));

#else

    Q_UNUSED(raJ2000);
    Q_UNUSED(decJ2000);

#endif    
}

FITSView::FITSView(QWidget * parent, FITSMode fitsMode, FITSScale filterType) : QScrollArea(parent) , zoomFactor(1.2)
{

    grabGesture(Qt::PinchGesture);

    image_frame = new FITSLabel(this);
    image_data  = NULL;
    display_image = NULL;
    firstLoad = true;
    trackingBoxEnabled=false;
    trackingBoxUpdated=false;
    filter = filterType;
    mode = fitsMode;

    setBackgroundRole(QPalette::Dark);

    markerCrosshair.setX(0);
    markerCrosshair.setY(0);

    currentZoom = 0.0;
    markStars = false;

    setBaseSize(750,650);

    connect(image_frame, SIGNAL(newStatus(QString,FITSBar)), this, SIGNAL(newStatus(QString,FITSBar)));
    connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));
    connect(image_frame, SIGNAL(markerSelected(int,int)), this, SLOT(processMarkerSelection(int,int)));
    connect(&wcsWatcher, SIGNAL(finished()), this, SLOT(handleWCSCompletion()));

    image_frame->setMouseTracking(true);
    setMouseMode(selectMouse);//This is the default mode because the Focus and Align FitsViews should not be in dragMouse mode

    //if (fitsMode == FITS_GUIDE)
    //connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));

    // Default size
    //resize(INITIAL_W, INITIAL_H);
}

FITSView::~FITSView()
{
    delete(image_frame);
    delete(image_data);
    delete(display_image);
}

bool FITSView::loadFITS (const QString &inFilename , bool silent)
{
    QProgressDialog fitsProg(this);

    bool setBayerParams=false;

    BayerParams param;
    if (image_data && image_data->hasDebayer())
    {
        setBayerParams=true;
        image_data->getBayerParams(&param);
    }

    delete (image_data);
    image_data = NULL;

    filterStack.clear();
    filterStack.push(FITS_NONE);
    if (filter != FITS_NONE)
        filterStack.push(filter);

    image_data = new FITSData(mode);

    if (setBayerParams)
        image_data->setBayerParams(&param);

    if (mode == FITS_NORMAL)
    {
        fitsProg.setWindowModality(Qt::WindowModal);
        fitsProg.setLabelText(i18n("Please hold while loading FITS file..."));
        fitsProg.setWindowTitle(i18n("Loading FITS"));
        fitsProg.setValue(10);
        qApp->processEvents();
    }

    if (image_data->loadFITS(inFilename, silent) == false)
        return false;


    if (mode == FITS_NORMAL)
    {
        if (fitsProg.wasCanceled())
            return false;
        else
        {
            fitsProg.setValue(65);
            qApp->processEvents();
        }
    }

    emit debayerToggled(image_data->hasDebayer());

    image_data->getDimensions(&currentWidth, &currentHeight);

    image_width  = currentWidth;
    image_height = currentHeight;

    image_frame->setSize(image_width, image_height);

    //hasWCS = image_data->hasWCS();

    maxPixel = image_data->getMax();
    minPixel = image_data->getMin();

    if (mode == FITS_NORMAL)
    {
        if (fitsProg.wasCanceled())
            return false;
        else
        {
            QFuture<bool> future = QtConcurrent::run(image_data, &FITSData::checkWCS);
            wcsWatcher.setFuture(future);
            fitsProg.setValue(75);
            qApp->processEvents();
        }
    }

    initDisplayImage();

    // Rescale to fits window
    if (firstLoad)
    {
        currentZoom   = 100;

        if (rescale(ZOOM_FIT_WINDOW))
            return false;

        firstLoad = false;
    }
    else
    {
        if (rescale(ZOOM_KEEP_LEVEL))
            return false;
    }

    if (mode == FITS_NORMAL)
    {
        if (fitsProg.wasCanceled())
            return false;
        else
        {
            fitsProg.setValue(100);
            qApp->processEvents();
        }
    }

    starsSearched = false;

    setAlignment(Qt::AlignCenter);

    if (isVisible())
        emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);

    return true;
}

int FITSView::saveFITS( const QString &newFilename )
{
    return image_data->saveFITS(newFilename);
}


int FITSView::rescale(FITSZoom type)
{
    switch (image_data->getDataType())
    {
        case TBYTE:
            return rescale<uint8_t>(type);
            break;

        case TSHORT:
            return rescale<int16_t>(type);
            break;

        case TUSHORT:
            return rescale<uint16_t>(type);
            break;

        case TLONG:
            return rescale<int32_t>(type);
            break;

        case TULONG:
            return rescale<uint32_t>(type);
            break;

        case TFLOAT:
            return rescale<float>(type);
            break;

        case TLONGLONG:
            return rescale<int64_t>(type);
            break;

        case TDOUBLE:
            return rescale<double>(type);
        break;

        default:
        break;
    }

    return 0;
}

template<typename T>  int FITSView::rescale(FITSZoom type)
{
    double val=0;
    double bscale, bzero;
    double min, max;
    bool displayBuffer = false;

    uint8_t *image_buffer = image_data->getImageBuffer();

    uint32_t size = image_data->getSize();
    int BBP = image_data->getBytesPerPixel();

    filter = filterStack.last();

    if (Options::autoStretch() && (filter == FITS_NONE || (filter >= FITS_ROTATE_CW && filter <= FITS_FLIP_V )))
    {
        image_buffer = new uint8_t[image_data->getSize() * image_data->getNumOfChannels() * BBP];
        memcpy(image_buffer, image_data->getImageBuffer(), image_data->getSize() * image_data->getNumOfChannels() * BBP);

        displayBuffer = true;

        float data_min   = -1;
        float data_max   = -1;

        image_data->applyFilter(FITS_AUTO_STRETCH, image_buffer, &data_min, &data_max);

        min = data_min;
        max = data_max;
    }
    else
        image_data->getMinMax(&min, &max);

    T *buffer = reinterpret_cast<T*>(image_buffer);

    if (min == max)
    {
        display_image->fill(Qt::white);
        emit newStatus(i18n("Image is saturated!"), FITS_MESSAGE);
    }
    else
    {
        bscale = 255. / (max - min);
        bzero  = (-min) * (255. / (max - min));

        if (image_height != image_data->getHeight() || image_width != image_data->getWidth())
        {
            image_width  = image_data->getWidth();
            image_height = image_data->getHeight();

            initDisplayImage();

            if (isVisible())
                emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);
        }

        image_frame->setScaledContents(true);
        currentWidth  = display_image->width();
        currentHeight = display_image->height();

        if (image_data->getNumOfChannels() == 1)
        {
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < image_height; j++)
            {
                unsigned char *scanLine = display_image->scanLine(j);

                for (int i = 0; i < image_width; i++)
                {
                    val = buffer[j * image_width + i] * bscale + bzero;
                    scanLine[i]= qBound(0.0, val, 255.0);
                }
            }
        }
        else
        {
            double rval=0,gval=0,bval=0;
            QRgb value;
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < image_height; j++)
            {
                QRgb *scanLine = reinterpret_cast<QRgb*>((display_image->scanLine(j)));

                for (int i = 0; i < image_width; i++)
                {
                    rval = buffer[j * image_width + i];
                    gval = buffer[j * image_width + i + size];
                    bval = buffer[j * image_width + i + size * 2];

                    value = qRgb(rval* bscale + bzero, gval* bscale + bzero, bval* bscale + bzero);

                    scanLine[i] = value;
                }
            }
        }
    }

    if (displayBuffer)
        delete [] image_buffer;

    switch (type)
    {
    case ZOOM_FIT_WINDOW:
        if ((display_image->width() > width() || display_image->height() > height()))
        {
            double w = baseSize().width() - BASE_OFFSET;
            double h = baseSize().height() - BASE_OFFSET;

            if(firstLoad == false)
            {
                w = viewport()->rect().width() - BASE_OFFSET;
                h = viewport()->rect().height() - BASE_OFFSET;
            }

            // Find the zoom level which will enclose the current FITS in the current window size
            //currentZoom = floor( (w / static_cast<double>(currentWidth)) * 10.) * 10.;
            currentZoom = floor( (w / static_cast<double>(currentWidth)) * 100.);

            /* If width is not the problem, try height */
            if (currentZoom > ZOOM_DEFAULT)
                //currentZoom = floor( (h / static_cast<double>(currentHeight)) * 10.) * 10.;
                currentZoom = floor( (h / static_cast<double>(currentHeight)) * 100.);

            currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
            currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

            if (currentZoom <= ZOOM_MIN)
                emit actionUpdated("view_zoom_out", false);
        }
        else
        {
            currentZoom   = 100;
            currentWidth  = image_width;
            currentHeight = image_height;

        }
        break;

    case ZOOM_KEEP_LEVEL:
    {
        currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
        currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

    }
        break;

    default:
        currentZoom   = 100;

        break;
    }

    setWidget(image_frame);


    if (type != ZOOM_KEEP_LEVEL)
        emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    return 0;

}

void FITSView::ZoomIn()
{

    if (currentZoom < ZOOM_DEFAULT)
        currentZoom += ZOOM_LOW_INCR;
    else
        currentZoom += ZOOM_HIGH_INCR;


    emit actionUpdated("view_zoom_out", true);
    if (currentZoom >= ZOOM_MAX)
    {
        currentZoom = ZOOM_MAX;
        emit actionUpdated("view_zoom_in", false);
    }

    currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
    currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

}

void FITSView::ZoomOut()
{

    if (currentZoom <= ZOOM_DEFAULT)
        currentZoom -= ZOOM_LOW_INCR;
    else
        currentZoom -= ZOOM_HIGH_INCR;

    if (currentZoom <= ZOOM_MIN)
    {
        currentZoom = ZOOM_MIN;
        emit actionUpdated("view_zoom_out", false);
    }

    emit actionUpdated("view_zoom_in", true);

    currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
    currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);
}

void FITSView::ZoomToFit()
{
    rescale(ZOOM_FIT_WINDOW);
    updateFrame();
}

void FITSView::updateFrame()
{

    QPixmap displayPixmap;
    bool ok=false;

    if (display_image == NULL)
        return;

    if (currentZoom != ZOOM_DEFAULT)
        ok = displayPixmap.convertFromImage(display_image->scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        ok = displayPixmap.convertFromImage(*display_image);


    if (ok == false)
        return;

    QPainter painter(&displayPixmap);

    drawOverlay(&painter);

    image_frame->setPixmap(displayPixmap);
    image_frame->resize(currentWidth,currentHeight);
}

void FITSView::ZoomDefault()
{
    emit actionUpdated("view_zoom_out", true);
    emit actionUpdated("view_zoom_in", true);


    currentZoom   = ZOOM_DEFAULT;
    currentWidth  = image_width;
    currentHeight = image_height;

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    update();

}

void FITSView::drawOverlay(QPainter *painter)
{
    painter->setRenderHint( QPainter::Antialiasing, Options::useAntialias() );

    if (markStars)
        drawStarCentroid(painter);

    //if (mode == FITS_GUIDE)
    if (trackingBoxEnabled)
        drawTrackingBox(painter);

    if (markerCrosshair.isNull() == false)
        drawMarker(painter);

    if (showCrosshair)
        drawCrosshair(painter);

    if(showObjects)
        drawObjectNames(painter);

    if (showEQGrid)
        drawEQGrid(painter);

    if (showPixelGrid)
        drawPixelGrid(painter);


}

void FITSView::updateMode(FITSMode fmode)
{
    mode = fmode;
}

void FITSView::drawMarker(QPainter *painter)
{
    painter->setPen( QPen( QColor( KStarsData::Instance()->colorScheme()->colorNamed("TargetColor" ) ) ) );
    painter->setBrush( Qt::NoBrush );
    float pxperdegree = (currentZoom/ZOOM_DEFAULT)* (57.3/1.8);

    float s1 = 0.5*pxperdegree;
    float s2 = pxperdegree;
    float s3 = 2.0*pxperdegree;

    float x0 = markerCrosshair.x()  * (currentZoom / ZOOM_DEFAULT);
    float y0 = markerCrosshair.y()  * (currentZoom / ZOOM_DEFAULT);
    float x1 = x0 - 0.5*s1;  float y1 = y0 - 0.5*s1;
    float x2 = x0 - 0.5*s2;  float y2 = y0 - 0.5*s2;
    float x3 = x0 - 0.5*s3;  float y3 = y0 - 0.5*s3;

    //Draw radial lines
    painter->drawLine( QPointF(x1, y0), QPointF(x3, y0) );
    painter->drawLine( QPointF(x0+s2, y0), QPointF(x0+0.5*s1, y0) );
    painter->drawLine( QPointF(x0, y1), QPointF(x0, y3) );
    painter->drawLine( QPointF(x0, y0+0.5*s1), QPointF(x0, y0+s2) );
    //Draw circles at 0.5 & 1 degrees
    painter->drawEllipse( QRectF(x1, y1, s1, s1) );
    painter->drawEllipse( QRectF(x2, y2, s2, s2) );
}

void FITSView::drawStarCentroid(QPainter *painter)
{
    painter->setPen(QPen(Qt::red, 2));

    int x1,y1, w;

    // image_data->getStarCenter();

    QList<Edge*> starCenters = image_data->getStarCenters();

    for (int i=0; i < starCenters.count() ; i++)
    {
        x1 = (starCenters[i]->x - starCenters[i]->width/2) * (currentZoom / ZOOM_DEFAULT);
        y1 = (starCenters[i]->y - starCenters[i]->width/2) * (currentZoom / ZOOM_DEFAULT);
        w = (starCenters[i]->width) * (currentZoom / ZOOM_DEFAULT);

        painter->drawEllipse(x1, y1, w, w);
    }
}

void FITSView::drawTrackingBox(QPainter *painter)
{
    painter->setPen(QPen(Qt::green, 2));

    if (trackingBox.isNull())
        return;

    int x1 = trackingBox.x() * (currentZoom / ZOOM_DEFAULT);
    int y1 = trackingBox.y() * (currentZoom / ZOOM_DEFAULT);
    int w  = trackingBox.width() * (currentZoom / ZOOM_DEFAULT);
    int h  = trackingBox.height() * (currentZoom / ZOOM_DEFAULT);

    painter->drawRect(x1, y1, w, h);
}

/**
This Method draws a large Crosshair in the center of the image, it is like a set of axes.
 */

void FITSView::drawCrosshair(QPainter *painter){

    float scale=(currentZoom / ZOOM_DEFAULT);
    QPointF c=QPointF(image_width/2 *scale,image_height/2 * scale);
    float midX=image_width/2 * scale;
    float midY=image_height/2 * scale;
    float maxX=image_width * scale;
    float maxY=image_height * scale;
    float r=50*scale;


     painter->setPen( QPen( QColor( KStarsData::Instance()->colorScheme()->colorNamed("TargetColor" ) ) ) );

    //Horizontal Line to Circle
    painter->drawLine(0, midY, midX - r, midY);

    //Horizontal Line past Circle
    painter->drawLine(midX + r, midY, maxX, midY);

    //Vertical Line to Circle
    painter->drawLine(midX, 0, midX, midY - r);

    //Vertical Line past Circle
    painter->drawLine(midX, midY + r, midX, maxY);

    //Circles
    painter->drawEllipse(c,r,r);
    painter->drawEllipse(c, r/2, r/2);

}

/**
This method is intended to draw a pixel grid onto the image.  It first determines useful information
from the image.  Then it draws the axes on the image if the crosshairs are not displayed.
Finally it draws the gridlines so that there will be 4 Gridlines on either side of the axes.
Note: This has to start drawing at the center not at the edges because the center axes must
be in the center of the image.
 */

void FITSView::drawPixelGrid(QPainter *painter){
    float scale=(currentZoom / ZOOM_DEFAULT);
    double width=image_width*scale;
    double height=image_height*scale;
    double cX=width/2;
    double cY=height/2;
    double deltaX=width/10;
    double deltaY=height/10;
    //draw the Axes
    painter->setPen(QPen(Qt::red));
    painter->drawText(cX-30,height-5,QString::number((int)((cX)/scale)));
    painter->drawText(width-30,cY-5,QString::number((int)((cY)/scale)));
    if(!showCrosshair)
    {
        painter->drawLine(cX,0,cX,height);
        painter->drawLine(0,cY,width,cY);
    }
    painter->setPen(QPen(Qt::gray));
    //Start one iteration past the Center and draw 4 lines on either side of 0
    for(int x=deltaX;x<cX-deltaX;x+=deltaX){
        painter->drawText(cX+x-30,height-5,QString::number((int)((cX+x)/scale)));
        painter->drawText(cX-x-30,height-5,QString::number((int)((cX-x)/scale)));
        painter->drawLine(cX-x,0,cX-x,height);
        painter->drawLine(cX+x,0,cX+x,height);
    }
    //Start one iteration past the Center and draw 4 lines on either side of 0
    for(int y=deltaY;y<cY-deltaY;y+=deltaY){
        painter->drawText(width-30,cY+y-5,QString::number((int)((cY+y)/scale)));
        painter->drawText(width-30,cY-y-5,QString::number((int)((cY-y)/scale)));
        painter->drawLine(0,cY+y,width,cY+y);
        painter->drawLine(0,cY-y,width,cY-y);
    }
}

bool FITSView::imageHasWCS(){
    return hasWCS;
}

void FITSView::drawObjectNames(QPainter *painter)
{
    painter->setPen( QPen( QColor( KStarsData::Instance()->colorScheme()->colorNamed("FITSObjectLabelColor" ) ) ) );
    float scale=(currentZoom / ZOOM_DEFAULT);
    foreach(FITSSkyObject *listObject, image_data->getSkyObjects())
    {
        painter->drawRect(listObject->x()*scale-5,listObject->y()*scale-5,10,10);
        painter->drawText(listObject->x()*scale+10,listObject->y()*scale+10,listObject->skyObject()->name());
    }
}

/**
This method will paint EQ Gridlines in an overlay if there is WCS data present.
It determines the minimum and maximum RA and DEC, then it uses that information to
judge which gridLines to draw.  Then it calls the drawEQGridlines methods below
to draw gridlines at those specific RA and Dec values.
 */

void FITSView::drawEQGrid(QPainter *painter){

   if (image_data->hasWCS())
       {
           wcs_point * wcs_coord = image_data->getWCSCoord();
           if (wcs_coord)
           {
               int size=image_width*image_height;
               double maxRA=-1000; double minRA=1000; double maxDec=-1000; double minDec=1000;

               for(int i=0;i<(size);i++)
               {
                   double ra = wcs_coord[i].ra;
                   double dec = wcs_coord[i].dec;
                   if(ra>maxRA)
                       maxRA=ra;
                   if(ra<minRA)
                       minRA=ra;
                   if(dec>maxDec)
                       maxDec=dec;
                   if(dec<minDec)
                       minDec=dec;
               }
               painter->setPen( QPen( Qt::yellow) );

               if (maxDec>80){
                   int minRAMinutes=(int)(minRA/15);//This will force the scale to whole hours of RA near the pole
                   int maxRAMinutes=(int)(maxRA/15);
                   for(int targetRA=minRAMinutes;targetRA<=maxRAMinutes;targetRA++)
                       drawEQGridlineAtRA(painter,wcs_coord,targetRA*15);
               }else{
                   int minRAMinutes=(int)(minRA/15*60);//This will force the scale to whole minutes of RA
                   int maxRAMinutes=(int)(maxRA/15*60);
                   for(int targetRA=minRAMinutes;targetRA<=maxRAMinutes;targetRA++)
                       drawEQGridlineAtRA(painter,wcs_coord,targetRA*15/60.0);
               }


               int minDecMinutes=(int)(minDec*4);//This will force the Dec Scale to 15 arc minutes
               int maxDecMinutes=(int)(maxDec*4);
               for(int targetDec=minDecMinutes;targetDec<=maxDecMinutes;targetDec++)
                    drawEQGridlineAtDec(painter,wcs_coord,targetDec/4.0);
           }
       }
}

void FITSView::drawEQGridlineAtRA(QPainter *painter,wcs_point *wcs_coord, double target){
    drawEQGridline(painter,wcs_coord,true,target);
}

void FITSView::drawEQGridlineAtDec(QPainter *painter,wcs_point *wcs_coord, double target){
    drawEQGridline(painter,wcs_coord,false,target);
}

/**
This method is intended to search all the sides of an image to locate two points that have the target
RA or DEC value and then draw a gridLine connecting thost two points.  There should be exactly 2.
If it fails to find two points, it just doesn't draw a line.  It makes heavy use of the helper method
pointIsNearWCSTargetPoint to simplify the if statements, which is defined below.
 */

void FITSView::drawEQGridline(QPainter *painter,wcs_point *wcs_coord, bool isRA, double target){
    float scale=(currentZoom / ZOOM_DEFAULT);
    int num=0;
    QPoint pt[2];
    bool vertical=true;
    //Search along top of image
    for(int x=1;x<image_width-1;x++){
        int y=1;
        if(pointIsNearWCSTargetPoint(wcs_coord,target,x,y,isRA,!vertical)){
            pt[num] = QPoint(x * scale,y * scale);
            num++;
            break;
        }
    }
    //Search along left side of image
    for(int y=1;y<image_height-1;y++){
            int x=1;
            if(pointIsNearWCSTargetPoint(wcs_coord,target,x,y,isRA,vertical)){
                    pt[num] = QPoint(x * scale,y * scale);
                    num++;
                    break;

            }
    }
    //Search along bottom of image
    for(int x=1;x<image_width-1;x++){
        int y=image_height-2;
        if(pointIsNearWCSTargetPoint(wcs_coord,target,x,y,isRA,!vertical)){
            pt[num] = QPoint(x * scale,y * scale);
            num++;
            break;
        }
    }
    //Search along right side of image
        for(int y=1;y<image_height-1;y++){
            int x=image_width-2;
            if(pointIsNearWCSTargetPoint(wcs_coord,target,x,y,isRA,vertical)){
                    pt[num] = QPoint(x * scale,y * scale);
                    num++;
                    break;
            }
        }
    if(num==2){
        if(isRA)
            painter->drawText(pt[1].x()-40,pt[1].y()-10,QString::number(dms(target).hour())+"h "+QString::number(dms(target).minute())+"'");
        else
            painter->drawText(pt[1].x()-40,pt[1].y()-10,QString::number(dms(target).degree())+"° "+QString::number(dms(target).arcmin())+"'");
        painter->drawLine(pt[0],pt[1]);
    }
}

/**
This method is intended to help find the X,Y Coordinates of a certain RA or DEC in an image.
Since the exact RA or DEC we are looking for probably does not match the exact RA or DEC for
a specific pixel, we have to compare the RA or DEC that we are looking for to the RA and DEC
in the WCS info of the surrounding pixels.  The method returns true if the position
 approximates the target RA or DEC.  To use the method, you need to specify the target number,
 whether you are looking horizontally or vertically, and whether you are looking at RA or DEC,
 in addition to giving the wcs_coord array and the x and y coordinates.
 */

bool FITSView::pointIsNearWCSTargetPoint(wcs_point *wcs_coord, double target, int x, int y, bool isRA, bool vertical){
    int i = x + y * image_width;
    int shift;//The Number of index values to the next point to compare
    if(vertical)
        shift=image_width;//Vertically it is a full row to the next point
    else
        shift=1;//Horizontally it is the next point
    double nextPoint;
    double prevPoint;
    if(isRA){
        nextPoint=wcs_coord[i+shift].ra;
        prevPoint=wcs_coord[i-shift].ra;
    } else{
        nextPoint=wcs_coord[i+shift].dec;
        prevPoint=wcs_coord[i-shift].dec;
    }

   return (target>nextPoint&&target<prevPoint)||(target>prevPoint&&target<nextPoint);
}

void FITSView::setFirstLoad(bool value)
{
    firstLoad = value;
}

QPixmap & FITSView::getTrackingBoxPixmap()
{
    if (trackingBox.isNull())
        return trackingBoxPixmap;

    int x1 = trackingBox.x() * (currentZoom / ZOOM_DEFAULT);
    int y1 = trackingBox.y() * (currentZoom / ZOOM_DEFAULT);
    int w  = trackingBox.width() * (currentZoom / ZOOM_DEFAULT);
    int h  = trackingBox.height() * (currentZoom / ZOOM_DEFAULT);

    trackingBoxPixmap = image_frame->grab(QRect(x1, y1, w, h));

    return trackingBoxPixmap;
}

void FITSView::setTrackingBox(const QRect & rect)
{
    if (rect != trackingBox)
    {
        trackingBoxUpdated=true;
        trackingBox = rect;
        updateFrame();
    }
}

bool FITSView::isCrosshairShown()
{
   return showCrosshair;
}

bool FITSView::isEQGridShown()
{
   return showEQGrid;
}

bool FITSView::areObjectsShown()
{
   return showObjects;
}

bool FITSView::isPixelGridShown()
{
   return showPixelGrid;
}

void FITSView::toggleCrosshair()
{
   showCrosshair=!showCrosshair;
   updateFrame();
}

void FITSView::toggleEQGrid()
{
    showEQGrid=!showEQGrid;
    updateFrame();

}
void FITSView::toggleObjects()
{
    showObjects=!showObjects;
    updateFrame();

}

void FITSView::togglePixelGrid()
{
    showPixelGrid=!showPixelGrid;
    updateFrame();

}

int FITSView::findStars(StarAlgorithm algorithm)
{
    int count = 0;

    if (trackingBoxEnabled)
    {
        switch (algorithm)
        {
        case ALGORITHM_GRADIENT:
            count = FITSData::findCannyStar(image_data, trackingBox);
            break;

        case ALGORITHM_CENTROID:
            count = image_data->findStars(trackingBox);
            break;

        case ALGORITHM_THRESHOLD:
            count = image_data->findOneStar(trackingBox);
            break;
        }
    }
    /*else if (algorithm == ALGORITHM_GRADIENT)
    {
        QRect boundary(0,0, image_data->getWidth(), image_data->getHeight());
        count = FITSData::findCannyStar(image_data, boundary);
    }*/
    else
    {
        count = image_data->findStars();
    }

    starAlgorithm = algorithm;

    starsSearched = true;

    return count;
}

void FITSView::toggleStars(bool enable)
{
    markStars = enable;

    if (markStars == true && starsSearched == false)
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        emit newStatus(i18n("Finding stars..."), FITS_MESSAGE);
        qApp->processEvents();
        int count = -1;

        count = findStars(starAlgorithm);

        if (count >= 0 && isVisible())
            emit newStatus(i18np("1 star detected.", "%1 stars detected.", count), FITS_MESSAGE);
        QApplication::restoreOverrideCursor();
    }
}

void FITSView::processPointSelection(int x, int y)
{
    //if (mode != FITS_GUIDE)
    //return;

    //image_data->getCenterSelection(&x, &y);

    //setGuideSquare(x,y);
    emit trackingStarSelected(x,y);
}

void FITSView::processMarkerSelection(int x, int y)
{
    markerCrosshair.setX(x);
    markerCrosshair.setY(y);

    updateFrame();
}

void FITSView::setTrackingBoxEnabled(bool enable)
{
    if (enable != trackingBoxEnabled)
    {
        trackingBoxEnabled = enable;
        updateFrame();
    }
}

void FITSView::wheelEvent(QWheelEvent* event)
{
    //This attempts to send the wheel event back to the Scroll Area if it was taken from a trackpad
    //It should still do the zoom if it is a mouse wheel
    if(event->source()==Qt::MouseEventSynthesizedBySystem){
        QScrollArea::wheelEvent(event);
    } else{
        QPoint mouseCenter=getImagePoint(event->pos());
        if (event->angleDelta().y() > 0)
            ZoomIn();
        else
            ZoomOut();
        event->accept();
        cleanUpZoom(mouseCenter);
    }
}

/**
This method is intended to keep key locations in an image centered on the screen while zooming.
If there is a marker or tracking box, it centers on those.  If not, it uses the point called
viewCenter that was passed as a parameter.
 */

void FITSView::cleanUpZoom(QPoint viewCenter)
{
    int x0=0;
    int y0=0;
    double scale = (currentZoom / ZOOM_DEFAULT);
    if (markerCrosshair.isNull() == false)
    {
        x0 = markerCrosshair.x()  * scale;
        y0 = markerCrosshair.y()  * scale;
    }
    else if (trackingBoxEnabled)
    {
        x0 = trackingBox.center().x()  * scale;
        y0 = trackingBox.center().y()  * scale;
    } else
    {
        x0 = viewCenter.x()  * scale;
        y0 = viewCenter.y()  * scale;
    }
    ensureVisible(x0,y0, width()/2 , height()/2);
    updateMouseCursor();
}

/**
This method converts a point from the ViewPort Coordinate System to the
Image Coordinate System.
 */

QPoint FITSView::getImagePoint(QPoint viewPortPoint)
{
    double scale = (currentZoom / ZOOM_DEFAULT);
    QPoint widgetPoint = widget()->mapFromParent(viewPortPoint);
    QPoint imagePoint = QPoint(widgetPoint.x() / scale , widgetPoint.y() / scale);
    return imagePoint;
}

void FITSView::initDisplayImage()
{
    delete (display_image);
    display_image = NULL;

    if (image_data->getNumOfChannels() == 1)
    {
        display_image = new QImage(image_width, image_height, QImage::Format_Indexed8);

        display_image->setColorCount(256);
        for (int i=0; i < 256; i++)
            display_image->setColor(i, qRgb(i,i,i));
    }
    else
    {
        display_image = new QImage(image_width, image_height, QImage::Format_RGB32);
    }
}

/**
The Following two methods allow gestures to work with trackpads.
Specifically, we are targeting the pinch events, so that if one is generated,
Then the pinchTriggered method will be called.  If the event is not a pinch gesture,
then the event is passed back to the other event handlers.
 */

bool FITSView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
           return gestureEvent(static_cast<QGestureEvent*>(event));
    return QScrollArea::event(event);
}

bool FITSView::gestureEvent(QGestureEvent *event)
{

    if (QGesture *pinch = event->gesture(Qt::PinchGesture))
        pinchTriggered(static_cast<QPinchGesture *>(pinch));
    return true;
}

/**
This Method works with Trackpads to use the pinch gesture to scroll in and out
It stores a point to keep track of the location where the gesture started so that
while you are zooming, it tries to keep that initial point centered in the view.
**/
void FITSView::pinchTriggered(QPinchGesture *gesture)
{
    if(!zooming){
        zoomLocation=getImagePoint(mapFromGlobal(QCursor::pos()));
        zooming=true;
    }
    if (gesture->state() == Qt::GestureFinished) {
        zooming=false;
    }
    if(gesture->totalScaleFactor()>1)
        ZoomIn();
    else
        ZoomOut();
    cleanUpZoom(zoomLocation);

}

void FITSView::handleWCSCompletion()
{
    hasWCS = wcsWatcher.result();
    if(hasWCS)
          this->updateFrame();
    emit wcsToggled(hasWCS);
}
