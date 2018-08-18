
/***************************************************************************
                          XPlanetImageviewer.cpp  -  Based on: KStars Image Viwer by Thomas Kabelmann
                             -------------------
    begin                : Sun Aug 12, 2018
    copyright            : (C) 2018 by Robert Lancaster
    email                : rlancaste@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "xplanetimageviewer.h"
#include "Options.h"
#include "dialogs/timedialog.h"
#include <QtConcurrent>

#ifndef KSTARS_LITE
#include "kstars.h"
#endif

#ifndef KSTARS_LITE
#include <KMessageBox>
#endif

#include <QDesktopWidget>
#include <QFileDialog>
#include <QPainter>
#include <QResizeEvent>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QtWidgets/QSlider>
#include "skymap.h"
#include "kspaths.h"

#include <QUuid>
#include <sys/stat.h>

XPlanetImageLabel::XPlanetImageLabel(QWidget *parent) : QFrame(parent)
{
#ifndef KSTARS_LITE
    grabGesture(Qt::PinchGesture);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    setLineWidth(2);
#endif
}

void XPlanetImageLabel::setImage(const QImage &img)
{
#ifndef KSTARS_LITE
    m_Image = img;
    pix     = QPixmap::fromImage(m_Image);
#endif
}

void XPlanetImageLabel::invertPixels()
{
#ifndef KSTARS_LITE
    m_Image.invertPixels();
    pix = QPixmap::fromImage(m_Image.scaled(width(), height(), Qt::KeepAspectRatio));
#endif
}

void XPlanetImageLabel::paintEvent(QPaintEvent *)
{
#ifndef KSTARS_LITE
    QPainter p;
    p.begin(this);
    int x = 0;
    if (pix.width() < width())
        x = (width() - pix.width()) / 2;
    p.drawPixmap(x, 0, pix);
    p.end();
#endif
}

void XPlanetImageLabel::resizeEvent(QResizeEvent *event)
{
    if (event->size() == pix.size())
        return;

    pix = QPixmap::fromImage(m_Image.scaled(event->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void XPlanetImageLabel::refreshImage()
{
    pix = QPixmap::fromImage(m_Image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    update();
}

void XPlanetImageLabel::wheelEvent(QWheelEvent *e)
{
    //This attempts to send the wheel event back to the Scroll Area if it was taken from a trackpad
    //It should still do the zoom if it is a mouse wheel
    if (e->source() == Qt::MouseEventSynthesizedBySystem)
    {
        QFrame::wheelEvent(e);
    }
    else
    {
        if (e->delta() > 0)
            emit zoomIn();
        else if (e->delta() < 0)
            emit zoomOut();
        e->accept();
    }
}

bool XPlanetImageLabel::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
        return gestureEvent(dynamic_cast<QGestureEvent *>(event));
    return QFrame::event(event);
}

bool XPlanetImageLabel::gestureEvent(QGestureEvent *event)
{
    if (QGesture *pinch = event->gesture(Qt::PinchGesture))
        pinchTriggered(dynamic_cast<QPinchGesture *>(pinch));
    return true;
}


void XPlanetImageLabel::pinchTriggered(QPinchGesture *gesture)
{
    if (gesture->totalScaleFactor() > 1)
        emit zoomIn();
    else
        emit zoomOut();
}


void XPlanetImageLabel::mousePressEvent(QMouseEvent *e)
{
     mouseButtonDown = true;
     lastMousePoint = e->globalPos();
     e->accept();
}

void XPlanetImageLabel::mouseReleaseEvent(QMouseEvent *e)
{
     mouseButtonDown = false;
     e->accept();
}

void XPlanetImageLabel::mouseMoveEvent(QMouseEvent *e)
{
    if(mouseButtonDown)
    {
        QPoint newPoint = e->globalPos();
        int dx = newPoint.x() - lastMousePoint.x();
        int dy = newPoint.y() - lastMousePoint.y();
        emit changePosition(QPoint(dx, dy));
        lastMousePoint = newPoint;
    }
    e->accept();
}

XPlanetImageViewer::XPlanetImageViewer(const QString &obj, QWidget *parent): QDialog(parent)
{
#ifndef KSTARS_LITE
    lastURL = QUrl::fromLocalFile(QDir::homePath());

    object=obj;
    #ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    #endif
    setAttribute(Qt::WA_DeleteOnClose, true);
    setModal(false);
    setWindowTitle(i18n("XPlanet Solar System Simulator: %1", object));

    setXPlanetDate(KStarsData::Instance()->ut());
    if (Options::xplanetFOV())
        FOV = KStars::Instance()->map()->fov();
    else
        FOV = 0;

    // Create widget
    QFrame *page = new QFrame(this);

    //setMainWidget( page );
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(page);
    setLayout(mainLayout);

    QWidget *selectorsWidget= new QWidget(this);
    QHBoxLayout *selectorsLayout = new QHBoxLayout(selectorsWidget);
    selectorsLayout->setMargin(0);
    mainLayout->addWidget(selectorsWidget);

    QStringList objects;
    objects << i18n("Sun") << i18n("Mercury") << i18n("Venus");
    objects << i18n("Earth") << i18n("Moon");
    objects << i18n("Mars") << i18n("Phobos") << i18n("Deimos");
    objects << i18n("Jupiter") << i18n("Ganymede") << i18n("Io") << i18n("Callisto") << i18n("Europa");
    objects << i18n("Saturn") << i18n("Titan") << i18n("Mimas") << i18n("Enceladus") << i18n("Tethys") << i18n("Dione") << i18n("Rhea") << i18n("Hyperion") << i18n("Iapetus") << i18n("Phoebe");
    objects << i18n("Uranus") << i18n("Umbriel") << i18n("Ariel") << i18n("Miranda") << i18n("Titania") << i18n("Oberon");
    objects << i18n("Neptune") << i18n("Triton");

    QComboBox *objectSelector = new QComboBox(this);
    objectSelector->addItems(objects);
    objectSelector->setToolTip(i18n("This allows you to select a new object/target for XPlanet to view"));
    selectorsLayout->addWidget(objectSelector);
    objectSelector->setCurrentIndex(objectSelector->findText(object));
    connect(objectSelector,  SIGNAL(currentTextChanged(QString)), this, SLOT(updateXPlanetObject(QString)));

    origin = i18n("Earth");

    selectorsLayout->addWidget(new QLabel(i18n("from"),this));
    originSelector = new QComboBox(this);
    originSelector->addItems(objects);
    originSelector->setToolTip(i18n("This allows you to select a viewing location"));
    selectorsLayout->addWidget(originSelector);
    originSelector->setCurrentIndex(originSelector->findText(origin));
    connect(originSelector,  SIGNAL(currentTextChanged(QString)), this, SLOT(updateXPlanetOrigin(QString)));

    lat = Options::xplanetLatitude().toDouble();
    lon = Options::xplanetLongitude().toDouble();
    radius = 45;

    selectorsLayout->addWidget(new QLabel(i18n("Location:"), this));

    latDisplay = new QLabel(this);
    latDisplay->setToolTip(i18n("XPlanet Latitude, only valid when viewing the object from the same object"));
    latDisplay->setText(QString::number(lat));
    latDisplay->setDisabled(true);
    selectorsLayout->addWidget(latDisplay);

    selectorsLayout->addWidget(new QLabel(",", this));
    lonDisplay = new QLabel(this);
    lonDisplay->setToolTip(i18n("XPlanet Longitude, only valid when viewing the object from the same object"));
    lonDisplay->setText(QString::number(lon));
    lonDisplay->setDisabled(true);
    selectorsLayout->addWidget(lonDisplay);

    selectorsLayout->addWidget(new QLabel(",", this));
    radDisplay = new QLabel(this);
    radDisplay->setToolTip(i18n("XPlanet object radius in %, only valid when viewing the object from the same object"));
    radDisplay->setText(QString::number(radius));
    radDisplay->setDisabled(true);
    selectorsLayout->addWidget(radDisplay);

    freeRotate = new QPushButton(this);
    freeRotate->setIcon(QIcon::fromTheme("object-rotate-left"));
    freeRotate->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    freeRotate->setMaximumSize(QSize(32,32));
    freeRotate->setMinimumSize(QSize(32,32));
    freeRotate->setCheckable(true);
    freeRotate->setToolTip(i18n("Hover over target and freely rotate view with mouse in XPlanet Viewer"));
    selectorsLayout->addWidget(freeRotate);
    connect(freeRotate, SIGNAL(clicked()), this, SLOT(slotFreeRotate()));

    QPushButton *saveB = new QPushButton(this);
    saveB->setIcon(QIcon::fromTheme("document-save"));
    saveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    saveB->setMaximumSize(QSize(32,32));
    saveB->setMinimumSize(QSize(32,32));
    saveB->setToolTip(i18n("Save the image to disk"));
    selectorsLayout->addWidget(saveB);
    connect(saveB, SIGNAL(clicked()), this, SLOT(saveFileToDisk()));

    QWidget *viewControlsWidget= new QWidget(this);
    QHBoxLayout *viewControlsLayout = new QHBoxLayout(viewControlsWidget);
    viewControlsLayout->setMargin(0);
    mainLayout->addWidget(viewControlsWidget);

    viewControlsLayout->addWidget(new QLabel(i18n("FOV:"), this));

    FOVEdit = new NonLinearDoubleSpinBox();
    FOVEdit->setDecimals(4);
    QList<double> possibleValues;
    possibleValues << 0;
    for(double i=.001;i<100;i*=1.5)
        possibleValues << i;
    FOVEdit->setRecommendedValues(possibleValues);
    FOVEdit->setValue(0);
    FOVEdit->setToolTip(i18n("Sets the FOV to the Specified value.   Note: has no effect if hovering over object."));
    viewControlsLayout->addWidget(FOVEdit);
    connect(FOVEdit,  SIGNAL(valueChanged(double)), this, SLOT(updateXPlanetFOVEdit()));

    kstarsFOV = new QPushButton(this);
    kstarsFOV->setIcon(QIcon::fromTheme("zoom-fit-width"));
    kstarsFOV->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    kstarsFOV->setMaximumSize(QSize(32,32));
    kstarsFOV->setMinimumSize(QSize(32,32));
    kstarsFOV->setToolTip(i18n("Zoom to the current KStars FOV.   Note: has no effect if hovering over object."));
    viewControlsLayout->addWidget(kstarsFOV);
    connect(kstarsFOV, SIGNAL(clicked()), this, SLOT(setKStarsXPlanetFOV()));

    noFOV = new QPushButton(this);
    noFOV->setIcon(QIcon::fromTheme("system-reboot"));
    noFOV->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    noFOV->setMaximumSize(QSize(32,32));
    noFOV->setMinimumSize(QSize(32,32));
    noFOV->setToolTip(i18n("Optimum FOV for the target, FOV parameter not specified.  Note: has no effect if hovering over object."));
    viewControlsLayout->addWidget(noFOV);
    connect(noFOV, SIGNAL(clicked()), this, SLOT(clearXPlanetFOV()));

    rotation = 0;

    viewControlsLayout->addWidget(new QLabel(i18n("Rotation:"), this));

    rotateEdit = new QSpinBox();

    rotateEdit->setRange(-180, 180);
    rotateEdit->setValue(0);
    rotateEdit->setSingleStep(10);
    rotateEdit->setToolTip(i18n("Set the view rotation to the desired angle"));
    viewControlsLayout->addWidget(rotateEdit);
    connect(rotateEdit,  SIGNAL(valueChanged(int)), this, SLOT(updateXPlanetRotationEdit()));

    QPushButton *invertRotation = new QPushButton(this);
    invertRotation->setIcon(QIcon::fromTheme("object-flip-vertical"));
    invertRotation->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    invertRotation->setMaximumSize(QSize(32,32));
    invertRotation->setMinimumSize(QSize(32,32));
    invertRotation->setToolTip(i18n("Rotate the view 180 degrees"));
    viewControlsLayout->addWidget(invertRotation);
    connect(invertRotation, SIGNAL(clicked()), this, SLOT(invertXPlanetRotation()));

    QPushButton *resetRotation = new QPushButton(this);
    resetRotation->setIcon(QIcon::fromTheme("system-reboot"));
    resetRotation->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetRotation->setMaximumSize(QSize(32,32));
    resetRotation->setMinimumSize(QSize(32,32));
    resetRotation->setToolTip(i18n("Reset view rotation to 0"));
    viewControlsLayout->addWidget(resetRotation);
    connect(resetRotation, SIGNAL(clicked()), this, SLOT(resetXPlanetRotation()));

    QPushButton *optionsB = new QPushButton(this);
    optionsB->setIcon(QIcon::fromTheme("configure"));
    optionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    optionsB->setMaximumSize(QSize(32,32));
    optionsB->setMinimumSize(QSize(32,32));
    optionsB->setToolTip(i18n("Bring up XPlanet Options"));
    viewControlsLayout->addWidget(optionsB);
    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));

    QPushButton *invertB = new QPushButton(this);
    invertB->setIcon(QIcon::fromTheme("edit-select-invert"));
    invertB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    invertB->setMaximumSize(QSize(32,32));
    invertB->setMinimumSize(QSize(32,32));
    invertB->setToolTip(i18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects "
                             "only the display and not the saving."));
    viewControlsLayout->addWidget(invertB);
    connect(invertB, SIGNAL(clicked()), this, SLOT(invertColors()));

    QWidget *timeWidget= new QWidget(this);
    QHBoxLayout *timeLayout = new QHBoxLayout(timeWidget);
    mainLayout->addWidget(timeWidget);
    timeLayout->setMargin(0);

    xplanetTime = KStarsData::Instance()->lt();

    QPushButton *setTime = new QPushButton(this);
    setTime->setIcon(QIcon::fromTheme("clock"));
    setTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    setTime->setMaximumSize(QSize(32,32));
    setTime->setMinimumSize(QSize(32,32));
    setTime->setToolTip(i18n("Allows you to set the XPlanet time to a different date/time from KStars"));
    timeLayout->addWidget(setTime);
    connect(setTime, SIGNAL(clicked()), this, SLOT(setXPlanetTime()));

    QPushButton *kstarsTime = new QPushButton(this);
    kstarsTime->setIcon(QIcon::fromTheme("system-reboot"));
    kstarsTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    kstarsTime->setMaximumSize(QSize(32,32));
    kstarsTime->setMinimumSize(QSize(32,32));
    kstarsTime->setToolTip(i18n("Sets the XPlanet time to the current KStars time"));
    timeLayout->addWidget(kstarsTime);
    connect(kstarsTime, SIGNAL(clicked()), this, SLOT(setXPlanetTimetoKStarsTime()));

    XPlanetTimeDisplay = new QLabel(this);
    XPlanetTimeDisplay->setToolTip(i18n("Current XPlanet Time"));
    timeLayout->addWidget(XPlanetTimeDisplay);

    XPlanetTimeDisplay->setText(xplanetTime.date().toString() + ",  " + xplanetTime.time().toString());

    timeSlider = new QSlider(Qt::Horizontal, this);
    timeLayout->addWidget(timeSlider);
    timeSlider->setRange(-100, 100);
    timeSlider->setToolTip(i18n("This sets the time step from the current XPlanet time, good for viewing events"));
    connect(timeSlider,  SIGNAL(valueChanged(int)), this, SLOT(updateXPlanetTime(int)));

    timeEdit = new QSpinBox(this);
    timeEdit->setRange(-100, 100);
    timeEdit->setMaximumWidth(50);
    timeEdit->setToolTip(i18n("This sets the time step from the current XPlanet time"));
    timeLayout->addWidget(timeEdit);
    connect(timeEdit,  SIGNAL(valueChanged(int)), this, SLOT(updateXPlanetTimeEdit()));

    timeUnit = MINS;
    timeUnitsSelect = new QComboBox(this);
    timeLayout->addWidget(timeUnitsSelect);
    timeUnitsSelect->addItem(i18n("years"));
    timeUnitsSelect->addItem(i18n("months"));
    timeUnitsSelect->addItem(i18n("days"));
    timeUnitsSelect->addItem(i18n("hours"));
    timeUnitsSelect->addItem(i18n("mins"));
    timeUnitsSelect->addItem(i18n("secs"));
    timeUnitsSelect->setCurrentIndex(MINS);
    timeUnitsSelect->setToolTip(i18n("Lets you change the units for the timestep in the animation"));
    connect(timeUnitsSelect,  SIGNAL(currentIndexChanged(int)), this, SLOT(updateXPlanetTimeUnits(int)));

    XPlanetTimer = new QTimer(this);
    XPlanetTimer->setInterval(Options::xplanetAnimationDelay().toInt());
    connect(XPlanetTimer, SIGNAL(timeout()), this, SLOT(incrementXPlanetTime()));

    runTime = new QPushButton(this);
    runTime->setIcon(QIcon::fromTheme("media-playback-start"));
    runTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    runTime->setCheckable(true);
    runTime->setMaximumSize(QSize(32,32));
    runTime->setMinimumSize(QSize(32,32));
    runTime->setToolTip(i18n("Lets you run the animation"));
    timeLayout->addWidget(runTime);
    connect(runTime, SIGNAL(clicked()), this, SLOT(toggleXPlanetRun()));

    QPushButton *resetTime = new QPushButton(this);
    resetTime->setIcon(QIcon::fromTheme("system-reboot"));
    resetTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetTime->setMaximumSize(QSize(32,32));
    resetTime->setMinimumSize(QSize(32,32));
    resetTime->setToolTip(i18n("Resets the animation to 0 timesteps from the current XPlanet Time"));
    timeLayout->addWidget(resetTime);
    connect(resetTime, SIGNAL(clicked()), this, SLOT(resetXPlanetTime()));

    m_View = new XPlanetImageLabel(page);
    m_View->setAutoFillBackground(false);
    m_Caption = new QLabel(page);
    m_Caption->setAutoFillBackground(true);
    m_Caption->setFrameShape(QFrame::StyledPanel);
    m_Caption->setText(object);
    // Add layout
    QVBoxLayout *vlay = new QVBoxLayout(page);
    vlay->setSpacing(0);
    vlay->setMargin(0);
    vlay->addWidget(m_View);
    vlay->addWidget(m_Caption);


    connect(m_View, SIGNAL(zoomIn()), this, SLOT(zoomInXPlanetFOV()));
    connect(m_View, SIGNAL(zoomOut()), this, SLOT(zoomOutXPlanetFOV()));
    connect(m_View, SIGNAL(changePosition(QPoint)), this, SLOT(changeXPlanetPosition(QPoint)));

    //Reverse colors
    QPalette p = palette();
    p.setColor(QPalette::Window, palette().color(QPalette::WindowText));
    p.setColor(QPalette::WindowText, palette().color(QPalette::Window));
    m_Caption->setPalette(p);
    m_View->setPalette(p);

#ifdef Q_OS_OSX
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
#endif
    updateXPlanetTime(0);
    startXplanet();
#endif
}

XPlanetImageViewer::~XPlanetImageViewer()
{
    QApplication::restoreOverrideCursor();
}

void XPlanetImageViewer::startXplanet()
{
    if(xplanetRunning)
        return;

    //This means something failed in the file output
    if(!setupOutputFile())
        return;

    QString xPlanetLocation = Options::xplanetPath();
#ifdef Q_OS_OSX
    if (Options::xplanetIsInternal())
        xPlanetLocation   = QCoreApplication::applicationDirPath() + "/xplanet/bin/xplanet";
#endif

    // If Options::xplanetPath() is empty, return
    if (xPlanetLocation.isEmpty())
    {
        KMessageBox::error(nullptr, i18n("Xplanet binary path is empty in config panel."));
        return;
    }

    // If Options::xplanetPath() does not exist, return
    const QFileInfo xPlanetLocationInfo(xPlanetLocation);
    if (!xPlanetLocationInfo.exists() || !xPlanetLocationInfo.isExecutable())
    {
        KMessageBox::error(nullptr, i18n("The configured Xplanet binary does not exist or is not executable."));
        return;
    }

    // Create xplanet process
    QProcess *xplanetProc = new QProcess(this);

    // Add some options
    QStringList args;

    //This specifies the object to be viewed
    args << "-body" << object.toLower();
    //This is the date and time requested
    args << "-date" << date;
    //This is the glare from the sun
    args << "-glare" << Options::xplanetGlare();
    args << "-base_magnitude" << Options::xplanetMagnitude();
    //This is the correction for light's travel time.
    args << "-light_time";

    args << "-geometry" << Options::xplanetWidth() + 'x' + Options::xplanetHeight();

    if(FOV != 0)
        args << "-fov" << QString::number(FOV);
    //Need to convert to locale for places that don't use decimals??
        //args << "-fov" << fov.setNum(fov());//.replace('.', ',');

    //This rotates the view for different object angles
    args << "-rotate" << QString::number(rotation);

    if (Options::xplanetConfigFile())
        args << "-config" << Options::xplanetConfigFilePath();
    if (Options::xplanetStarmap())
        args << "-starmap" << Options::xplanetStarmapPath();
    if (Options::xplanetArcFile())
        args << "-arc_file" << Options::xplanetArcFilePath();
    if (!file.fileName().isEmpty())
        args << "-output" << file.fileName() << "-quality" << Options::xplanetQuality();

    // Labels
    if (Options::xplanetLabel())
    {
        args << "-fontsize" << Options::xplanetFontSize() << "-color"
             << "0x" + Options::xplanetColor().mid(1) << "-date_format" << Options::xplanetDateFormat();

        if (Options::xplanetLabelGMT())
            args << "-gmtlabel";
        else
            args << "-label";
        if (!Options::xplanetLabelString().isEmpty())
            args << "-label_string"
                 << "\"" + Options::xplanetLabelString() + "\"";
        if (Options::xplanetLabelTL())
            args << "-labelpos"
                 << "+15+15";
        else if (Options::xplanetLabelTR())
            args << "-labelpos"
                 << "-15+15";
        else if (Options::xplanetLabelBR())
            args << "-labelpos"
                 << "-15-15";
        else if (Options::xplanetLabelBL())
            args << "-labelpos"
                 << "+15-15";
    }

    // Markers
    if (Options::xplanetMarkerFile())
        args << "-marker_file" << Options::xplanetMarkerFilePath();
    if (Options::xplanetMarkerBounds())
        args << "-markerbounds" << Options::xplanetMarkerBoundsPath();

    // Position
    // This sets the position from which the planet is viewed.
    // Note that setting Latitude and Longitude means that position above the SAME object

    if(object == origin)
    {
        if (Options::xplanetRandom())
            args << "-random";
        else
            args << "-latitude" << QString::number(lat) << "-longitude" << QString::number(lon) << "-radius" << QString::number(radius);
    }
    else
        args << "-origin" << origin;

    // Projection
    if (Options::xplanetProjection())
    {
        switch (Options::xplanetProjection())
        {
        case 1:
            args << "-projection"
                 << "ancient";
            break;
        case 2:
            args << "-projection"
                 << "azimuthal";
            break;
        case 3:
            args << "-projection"
                 << "bonne";
            break;
        case 4:
            args << "-projection"
                 << "gnomonic";
            break;
        case 5:
            args << "-projection"
                 << "hemisphere";
            break;
        case 6:
            args << "-projection"
                 << "lambert";
            break;
        case 7:
            args << "-projection"
                 << "mercator";
            break;
        case 8:
            args << "-projection"
                 << "mollweide";
            break;
        case 9:
            args << "-projection"
                 << "orthographic";
            break;
        case 10:
            args << "-projection"
                 << "peters";
            break;
        case 11:
            args << "-projection"
                 << "polyconic";
            break;
        case 12:
            args << "-projection"
                 << "rectangular";
            break;
        case 13:
            args << "-projection"
                 << "tsc";
            break;
        default:
            break;
        }
        if (Options::xplanetBackground())
        {
            if (Options::xplanetBackgroundImage())
                args << "-background" << Options::xplanetBackgroundImagePath();
            else
                args << "-background"
                     << "0x" + Options::xplanetBackgroundColorValue().mid(1);
        }
    }

#ifdef Q_OS_OSX
    if (Options::xplanetIsInternal())
    {
        QString searchDir = QCoreApplication::applicationDirPath() + "/xplanet/share/xplanet/";
        args << "-searchdir" << searchDir;
    }
#endif

    //This prevents it from running forever.
    args << "-num_times" << "1";
    // Run xplanet

    if(Options::xplanetUseFIFO())
        QtConcurrent::run(this, &XPlanetImageViewer::loadImage);
    xplanetProc->start(xPlanetLocation, args);

   // qDebug() << "Run:" << xplanetProc->program() << args.join(" ");
    xplanetRunning = true;
    bool succeeded = xplanetProc->waitForFinished(Options::xplanetTimeout().toInt());
    xplanetRunning = false;
    xplanetProc->deleteLater();
    if(succeeded)
    {
        if(FOV == 0)
            m_Caption->setText(i18n("XPlanet View: ") + object + i18n(" from ") + origin + ",  " + dateText);
        else
            m_Caption->setText(i18n("XPlanet View: ") + object + i18n(" from ") + origin + ",  " + dateText + i18n(", FOV: ") + QString::number(FOV));
        if(!Options::xplanetUseFIFO())
            loadImage();
        showImage();
    }
}

bool XPlanetImageViewer::setupOutputFile()
{
    if(Options::xplanetUseFIFO())
    {
        if(file.fileName().contains("/tmp/xplanetfifo") && file.exists())
            return true;
        file.setFileName(QString("/tmp/xplanetfifo%1.png").arg(QUuid::createUuid().toString().mid(1, 8)));
        int fd =0;
        if ((fd = mkfifo(file.fileName().toLatin1(), S_IRUSR | S_IWUSR) < 0))
        {
            KMessageBox::error(nullptr, i18n("Error making FIFO file %1: %2.", file.fileName(), strerror(errno)));
            return false;
        }
    }
    else
    {
        QDir writableDir;
        QString xPlanetDirPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "xplanet";
        writableDir.mkpath(xPlanetDirPath);
        file.setFileName(xPlanetDirPath + QDir::separator() + object + ".png");
    }

    return true;
}

void XPlanetImageViewer::zoomInXPlanetFOV()
{
    if(origin == object)
    {
        radius += 5;
        radDisplay->setText(QString::number(radius));
        startXplanet();
    }
    else
    {
        FOVEdit->stepDown();
        startXplanet();
    }

}

void XPlanetImageViewer::zoomOutXPlanetFOV()
{
    if(origin == object)
    {
        if(radius > 0)
        {
            radius -= 5;
            radDisplay->setText(QString::number(radius));
            startXplanet();
        }
    }
    else
    {
        FOVEdit->stepUp();
        startXplanet();
    }

}

void XPlanetImageViewer::updateXPlanetTime(int timeShift){

    KStarsDateTime shiftedXPlanetTime;
    switch(timeUnit)
    {
        case YEARS:
            shiftedXPlanetTime = xplanetTime.addDays(timeShift * 365);
            break;

        case MONTHS:
            shiftedXPlanetTime = xplanetTime.addDays(timeShift * 30);
            break;

        case DAYS:
            shiftedXPlanetTime = xplanetTime.addDays(timeShift);
            break;

        case HOURS:
            shiftedXPlanetTime = xplanetTime.addSecs(timeShift * 3600);
            break;

        case MINS:
            shiftedXPlanetTime = xplanetTime.addSecs(timeShift * 60);
            break;

        case SECS:
            shiftedXPlanetTime = xplanetTime.addSecs(timeShift);
            break;
    }

    setXPlanetDate(shiftedXPlanetTime);
    dateText = shiftedXPlanetTime.date().toString() + ",  " + shiftedXPlanetTime.time().toString();
    timeEdit->setValue(timeShift);
    startXplanet();
}

void XPlanetImageViewer::updateXPlanetObject(const QString &obj){
    object = obj;

    setWindowTitle(i18n("XPlanet Solar System Simulator: %1", object));
    if(freeRotate->isChecked())
            originSelector->setCurrentIndex(originSelector->findText(object));

    startXplanet();
}

void XPlanetImageViewer::updateXPlanetOrigin(const QString &obj)
{
    origin = obj;
    if(object == origin)
        freeRotate->setChecked(true);
    else
        freeRotate->setChecked(false);
    updateStates();//This will update the disabled/enabled states
    startXplanet();
}

void XPlanetImageViewer::changeXPlanetPosition(QPoint delta)
{
    if(origin == object)
    {
        double newLon = lon + delta.x();
        double newLat = lat + delta.y();

        if(newLat > 90)
            newLat = 90;
        if(newLat < -90)
            newLat = -90;

        lon = newLon;
        lat = newLat;

        latDisplay->setText(QString::number(lat));
        lonDisplay->setText(QString::number(lon));
        startXplanet();
    }
}

void XPlanetImageViewer::slotFreeRotate()
{
    if(freeRotate->isChecked())
        originSelector->setCurrentIndex(originSelector->findText(object));
    else
        originSelector->setCurrentIndex(originSelector->findText(i18n("Earth")));
}

void XPlanetImageViewer::updateStates()
{
    if(freeRotate->isChecked())
    {
        FOVEdit->setDisabled(true);
        kstarsFOV->setDisabled(true);
        noFOV->setDisabled(true);

        latDisplay->setDisabled(false);
        lonDisplay->setDisabled(false);
        radDisplay->setDisabled(false);
    }
    else
    {
        FOVEdit->setDisabled(false);
        kstarsFOV->setDisabled(false);
        noFOV->setDisabled(false);

        latDisplay->setDisabled(true);
        lonDisplay->setDisabled(true);
        radDisplay->setDisabled(true);
    }
}

void XPlanetImageViewer::setXPlanetDate(KStarsDateTime time){
    //Note Xplanet uses UT time for everything but we want the labels to all be LT
    KStarsDateTime utTime = KStarsData::Instance()->geo()->LTtoUT(time);
    QString year, month, day, hour, minute, second;

    if (year.setNum(utTime.date().year()).size() == 1)
        year.push_front('0');
    if (month.setNum(utTime.date().month()).size() == 1)
        month.push_front('0');
    if (day.setNum(utTime.date().day()).size() == 1)
        day.push_front('0');
    if (hour.setNum(utTime.time().hour()).size() == 1)
        hour.push_front('0');
    if (minute.setNum(utTime.time().minute()).size() == 1)
        minute.push_front('0');
    if (second.setNum(utTime.time().second()).size() == 1)
        second.push_front('0');

    date = year + month + day + '.' + hour + minute + second;

}

void XPlanetImageViewer::updateXPlanetTimeUnits(int units)
{
    timeUnit = units;
    updateXPlanetTimeEdit();
}

void XPlanetImageViewer::updateXPlanetTimeEdit()
{
    int timeShift = timeEdit->text().toInt();
    if(timeSlider->value() != timeShift)
        timeSlider->setValue(timeShift);
    else
        updateXPlanetTime(timeShift);
}

void XPlanetImageViewer::incrementXPlanetTime()
{
    if(!xplanetRunning)
    {
        int timeShift = timeEdit->text().toInt();
        if(timeSlider->maximum() == timeShift)
            timeSlider->setMaximum(timeSlider->maximum() + 1);
        if(timeEdit->maximum() == timeShift)
            timeEdit->setMaximum(timeSlider->maximum() + 1);
        timeSlider->setValue(timeShift + 1);
    }
}

void XPlanetImageViewer::setXPlanetTime()
{
    QPointer<TimeDialog> timedialog = new TimeDialog(xplanetTime, KStarsData::Instance()->geo(), this);
    if (timedialog->exec() == QDialog::Accepted)
    {
        xplanetTime = timedialog->selectedDateTime();
        XPlanetTimeDisplay->setText(xplanetTime.date().toString() + ",  " + xplanetTime.time().toString());
        int timeShift = 0;
        timeSlider->setMaximum(100);
        if(timeSlider->value() != timeShift)
            timeSlider->setValue(timeShift);
        else
            updateXPlanetTime(timeShift);
        startXplanet();
    }
}

void XPlanetImageViewer::setXPlanetTimetoKStarsTime()
{
    xplanetTime = KStarsData::Instance()->lt();
    XPlanetTimeDisplay->setText(xplanetTime.date().toString() + ",  " + xplanetTime.time().toString());
    int timeShift = 0;
    timeSlider->setMaximum(100);
    if(timeSlider->value() != timeShift)
        timeSlider->setValue(timeShift);
    else
        updateXPlanetTime(timeShift);
    startXplanet();
}

void XPlanetImageViewer::resetXPlanetTime()
{
    int timeShift = 0;
    timeSlider->setMaximum(100);
    timeEdit->setMaximum(100);
    if(timeSlider->value() != timeShift)
        timeSlider->setValue(timeShift);
    else
        updateXPlanetTime(timeShift);
}

void XPlanetImageViewer::toggleXPlanetRun()
{
    if(XPlanetTimer->isActive())
    {
        XPlanetTimer->stop();
    }
    else
    {
        XPlanetTimer->setInterval(Options::xplanetAnimationDelay().toInt());
        XPlanetTimer->start();
    }
}

void XPlanetImageViewer::updateXPlanetFOVEdit()
{
    FOV = FOVEdit->value();
    startXplanet();
}

void XPlanetImageViewer::clearXPlanetFOV()
{
    FOV = 0;
    FOVEdit->setValue(0);
    startXplanet();
}

void XPlanetImageViewer::setKStarsXPlanetFOV()
{
    FOV = KStars::Instance()->map()->fov();
    FOVEdit->setValue(FOV);
    startXplanet();
}

void XPlanetImageViewer::updateXPlanetRotationEdit()
{
    rotation = rotateEdit->value();
    startXplanet();
}

void XPlanetImageViewer::resetXPlanetRotation(){
    rotateEdit->setValue(0);
}

void XPlanetImageViewer::invertXPlanetRotation(){
    rotateEdit->setValue(180);
}

bool XPlanetImageViewer::loadImage()
{
#ifndef KSTARS_LITE
    if (!image.load(file.fileName()))
    {
        QString text = i18n("Loading of the image of object %1 failed.", object);
        KMessageBox::error(this, text);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool XPlanetImageViewer::showImage()
{
#ifndef KSTARS_LITE

    //If the image is larger than screen width and/or screen height,
    //shrink it to fit the screen
    QRect deskRect = QApplication::desktop()->availableGeometry();
    int w          = deskRect.width();  // screen width
    int h          = deskRect.height(); // screen height

    if (image.width() <= w && image.height() > h) //Window is taller than desktop
        image = image.scaled(int(image.width() * h / image.height()), h);
    else if (image.height() <= h && image.width() > w) //window is wider than desktop
        image = image.scaled(w, int(image.height() * w / image.width()));
    else if (image.width() > w && image.height() > h) //window is too tall and too wide
    {
        //which needs to be shrunk least, width or height?
        float fx = float(w) / float(image.width());
        float fy = float(h) / float(image.height());
        if (fx > fy) //width needs to be shrunk less, so shrink to fit in height
            image = image.scaled(int(image.width() * fy), h);
        else //vice versa
            image = image.scaled(w, int(image.height() * fx));
    }
    const bool initialLoad = !isVisible();

    show(); // hide is default

    m_View->setImage(image);
    w = image.width();

    //If the caption is wider than the image, set the window size
    //to fit the caption
    if (m_Caption->width() > w)
        w = m_Caption->width();
    if(initialLoad)
        resize(w, image.height());
    else
    {
        m_View->refreshImage();
    }

    update();
    show();

    return true;
#else
    return false;
#endif
}

void XPlanetImageViewer::saveFileToDisk()
{
#ifndef KSTARS_LITE

    QUrl newURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save Image"), lastURL); // save-dialog with default filename
    if (!newURL.isEmpty())
    {
        if (newURL.toLocalFile().endsWith(QLatin1String(".png")) == false)
           newURL.setPath(newURL.toLocalFile() + ".png");

        QFile f(newURL.toLocalFile());
        if (f.exists())
        {
            int r = KMessageBox::warningContinueCancel(parentWidget(),
                                                       i18n("A file named \"%1\" already exists. "
                                                            "Overwrite it?",
                                                            newURL.fileName()),
                                                       i18n("Overwrite File?"), KStandardGuiItem::overwrite());
            if (r == KMessageBox::Cancel)
                return;

            f.remove();
        }

        lastURL = QUrl(newURL.toString(QUrl::RemoveFilename));

        saveFile(newURL);
    }
#endif
}

void XPlanetImageViewer::saveFile(QUrl &url)
{
#ifndef KSTARS_LITE
    if (file.copy(url.toLocalFile()) == false)
    {
        QString text = i18n("Saving of the image %1 failed.", url.toString());
        KMessageBox::error(this, text);
    }
    else
        KStars::Instance()->statusBar()->showMessage(i18n("Saved image to %1", url.toString()));
#endif
}

void XPlanetImageViewer::invertColors()
{
#ifndef KSTARS_LITE
    // Invert colors
    m_View->invertPixels();
    m_View->update();
#endif
}

