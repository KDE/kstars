/*
    SPDX-FileCopyrightText: Thomas Kabelmann
    SPDX-FileCopyrightText: 2018 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xplanetimageviewer.h"
#include "Options.h"
#include "dialogs/timedialog.h"
#include "ksnotification.h"

#include <QtConcurrent>

#ifndef KSTARS_LITE
#include "kstars.h"
#endif

#ifndef KSTARS_LITE
#include <KMessageBox>
#endif

#include <QFileDialog>
#include <QPainter>
#include <QResizeEvent>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QScreen>
#include <QSlider>
#include "skymap.h"
#include "kspaths.h"
#include "fov.h"

#include <QUuid>
#include <sys/stat.h>
#include <QInputDialog>

typedef enum
{
    SUN, MERCURY, VENUS,
    EARTH, MOON,
    MARS, PHOBOS, DEIMOS,
    JUPITER, GANYMEDE, IO, CALLISTO, EUROPA,
    SATURN, TITAN, MIMAS, ENCELADUS, TETHYS, DIONE, RHEA, HYPERION, IAPETUS, PHOEBE,
    URANUS, UMBRIEL, ARIEL, MIRANDA, TITANIA, OBERON,
    NEPTUNE, TRITON
} objects;

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
    m_Pix     = QPixmap::fromImage(m_Image);
#endif
}

void XPlanetImageLabel::invertPixels()
{
#ifndef KSTARS_LITE
    m_Image.invertPixels();
    m_Pix = QPixmap::fromImage(m_Image.scaled(width(), height(), Qt::KeepAspectRatio));
#endif
}

void XPlanetImageLabel::paintEvent(QPaintEvent *)
{
#ifndef KSTARS_LITE
    QPainter p;
    p.begin(this);
    int x = 0;
    if (m_Pix.width() < width())
        x = (width() - m_Pix.width()) / 2;
    p.drawPixmap(x, 0, m_Pix);
    p.end();
#endif
}

void XPlanetImageLabel::resizeEvent(QResizeEvent *event)
{
    if (event->size() == m_Pix.size())
        return;

    m_Pix = QPixmap::fromImage(m_Image.scaled(event->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void XPlanetImageLabel::refreshImage()
{
    m_Pix = QPixmap::fromImage(m_Image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    m_MouseButtonDown = true;
    m_LastMousePoint = e->globalPos();
    e->accept();
}

void XPlanetImageLabel::mouseReleaseEvent(QMouseEvent *e)
{
    m_MouseButtonDown = false;
    e->accept();
}

void XPlanetImageLabel::mouseMoveEvent(QMouseEvent *e)
{
    if(m_MouseButtonDown)
    {
        QPoint newPoint = e->globalPos();
        int dx = newPoint.x() - m_LastMousePoint.x();
        int dy = newPoint.y() - m_LastMousePoint.y();
        if(e->buttons() & Qt::RightButton)
            emit changeLocation(QPoint(dx, dy));
        if(e->buttons() & Qt::LeftButton)
            emit changePosition(QPoint(dx, dy));
        m_LastMousePoint = newPoint;
    }
    e->accept();
}

XPlanetImageViewer::XPlanetImageViewer(const QString &obj, QWidget *parent): QDialog(parent)
{
#ifndef KSTARS_LITE
    m_LastFile = QDir::homePath();

#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    setAttribute(Qt::WA_DeleteOnClose, true);
    setModal(false);
    setWindowTitle(i18nc("@title:window", "XPlanet Solar System Simulator: %1", obj));

    setXPlanetDate(KStarsData::Instance()->ut());

    // Create widget
    QFrame *page = new QFrame(this);

    //setMainWidget( page );
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(page);
    setLayout(mainLayout);

    QWidget *selectorsWidget = new QWidget(this);
    QHBoxLayout *selectorsLayout = new QHBoxLayout(selectorsWidget);
    selectorsLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(selectorsWidget);

    m_ObjectNames       << i18n("Sun")      << i18n("Mercury")  << i18n("Venus");
    m_objectDefaultFOVs << 0.74818          << 0.004          << 0.02;
    m_ObjectNames       << i18n("Earth")    << i18n("Moon");
    m_objectDefaultFOVs << 1.0              << 0.74818;
    m_ObjectNames       << i18n("Mars")     << i18n("Phobos")   << i18n("Deimos");
    m_objectDefaultFOVs << 0.00865          << 0.00002          << 0.00002;
    m_ObjectNames       << i18n("Jupiter")  << i18n("Ganymede") << i18n("Io")       << i18n("Callisto") << i18n("Europa");
    m_objectDefaultFOVs << 0.02             << 0.0005           << 0.0004           << 0.0005           << 0.0003;
    m_ObjectNames       << i18n("Saturn")   << i18n("Titan")    << i18n("Mimas")    << i18n("Enceladus") << i18n("Tethys")   << i18n("Dione")    << i18n("Rhea")     << i18n("Hyperion") << i18n("Iapetus")  << i18n("Phoebe");
    m_objectDefaultFOVs << 0.02             << 0.0003            << 0.00002            << 0.00003            << 0.00007            << 0.00007            << 0.0001            << 0.00002            << 0.0001            << 0.00002;
    m_ObjectNames       << i18n("Uranus")   << i18n("Umbriel")  << i18n("Ariel")    << i18n("Miranda")  << i18n("Titania")  << i18n("Oberon");
    m_objectDefaultFOVs << 0.00256          << 0.00004            << 0.00004            << 0.00002            << 0.00005            << 0.00005;
    m_ObjectNames       << i18n("Neptune")  << i18n("Triton");
    m_objectDefaultFOVs << 0.00114          << 0.0001;

    m_CurrentObjectIndex = m_ObjectNames.indexOf(obj);
    if (m_CurrentObjectIndex < 0)
        // Set to Saturn if current object is not in the list.
        m_CurrentObjectIndex = 13;
    m_ObjectName = m_ObjectNames.at(m_CurrentObjectIndex);

    QComboBox *objectSelector = new QComboBox(this);
    objectSelector->addItems(m_ObjectNames);
    objectSelector->setToolTip(i18n("This allows you to select a new object/target for XPlanet to view"));
    selectorsLayout->addWidget(objectSelector);
    objectSelector->setCurrentIndex(m_CurrentObjectIndex);
    connect(objectSelector,  SIGNAL(currentIndexChanged(int)), this, SLOT(updateXPlanetObject(int)));

    m_CurrentOriginIndex = EARTH;
    m_OriginName = m_ObjectNames.at(EARTH);

    selectorsLayout->addWidget(new QLabel(i18n("from"), this));
    m_OriginSelector = new QComboBox(this);
    m_OriginSelector->addItems(m_ObjectNames);
    m_OriginSelector->setToolTip(i18n("This allows you to select a viewing location"));
    selectorsLayout->addWidget(m_OriginSelector);
    m_OriginSelector->setCurrentIndex(EARTH);
    connect(m_OriginSelector,  SIGNAL(currentIndexChanged(int)), this, SLOT(updateXPlanetOrigin(int)));

    m_lat = Options::xplanetLatitude().toDouble();
    m_lon = Options::xplanetLongitude().toDouble();
    m_Radius = 45;

    selectorsLayout->addWidget(new QLabel(i18n("Location:"), this));

    m_PositionDisplay = new QLabel(this);
    m_PositionDisplay->setToolTip(i18n("XPlanet Latitude, Longitude, and object radius in %. This is only valid when viewing the object from the same object"));
    updatePositionDisplay();
    m_PositionDisplay->setDisabled(true);
    selectorsLayout->addWidget(m_PositionDisplay);

    QPushButton *resetXPlanetLocation = new QPushButton(this);
    resetXPlanetLocation->setIcon(QIcon::fromTheme("system-reboot"));
    resetXPlanetLocation->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetXPlanetLocation->setMaximumSize(QSize(32, 32));
    resetXPlanetLocation->setMinimumSize(QSize(32, 32));
    resetXPlanetLocation->setToolTip(i18n("Reset XPlanet Location to the location specified in the XPlanet Options"));
    selectorsLayout->addWidget(resetXPlanetLocation);
    connect(resetXPlanetLocation, SIGNAL(clicked()), this, SLOT(resetLocation()));

    m_FreeRotate = new QPushButton(this);
    m_FreeRotate->setIcon(QIcon::fromTheme("object-rotate-left"));
    m_FreeRotate->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    m_FreeRotate->setMaximumSize(QSize(32, 32));
    m_FreeRotate->setMinimumSize(QSize(32, 32));
    m_FreeRotate->setCheckable(true);
    m_FreeRotate->setToolTip(i18n("Hover over target and freely rotate view with mouse in XPlanet Viewer"));
    selectorsLayout->addWidget(m_FreeRotate);
    connect(m_FreeRotate, SIGNAL(clicked()), this, SLOT(slotFreeRotate()));

    QPushButton *reCenterB = new QPushButton(this);
    reCenterB->setIcon(QIcon::fromTheme("snap-bounding-box-center"));
    reCenterB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    reCenterB->setMaximumSize(QSize(32, 32));
    reCenterB->setMinimumSize(QSize(32, 32));
    reCenterB->setToolTip(i18n("Recenters the XPlanet image once it has been moved"));
    selectorsLayout->addWidget(reCenterB);
    connect(reCenterB, SIGNAL(clicked()), this, SLOT(reCenterXPlanet()));

    QPushButton *saveB = new QPushButton(this);
    saveB->setIcon(QIcon::fromTheme("document-save"));
    saveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    saveB->setMaximumSize(QSize(32, 32));
    saveB->setMinimumSize(QSize(32, 32));
    saveB->setToolTip(i18n("Save the image to disk"));
    selectorsLayout->addWidget(saveB);
    connect(saveB, SIGNAL(clicked()), this, SLOT(saveFileToDisk()));

    QWidget *viewControlsWidget = new QWidget(this);
    QHBoxLayout *viewControlsLayout = new QHBoxLayout(viewControlsWidget);
    viewControlsLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(viewControlsWidget);

    viewControlsLayout->addWidget(new QLabel(i18n("FOV:"), this));

    m_FOVEdit = new NonLinearDoubleSpinBox();
    m_FOVEdit->setDecimals(5);
    QList<double> possibleValues;
    possibleValues << 0;
    for(double i = .0001; i < 100; i *= 1.5)
        possibleValues << i;
    m_FOVEdit->setRecommendedValues(possibleValues);
    m_FOVEdit->setToolTip(i18n("Sets the FOV to the Specified value.   Note: has no effect if hovering over object."));
    viewControlsLayout->addWidget(m_FOVEdit);

    if (Options::xplanetFOV())
        m_FOV = KStars::Instance()->map()->fov();
    else
        m_FOV = m_objectDefaultFOVs.at( m_CurrentObjectIndex);
    m_FOVEdit->setValue(m_FOV);

    connect(m_FOVEdit,  SIGNAL(valueChanged(double)), this, SLOT(updateXPlanetFOVEdit()));

    m_KStarsFOV = new QPushButton(this);
    m_KStarsFOV->setIcon(QIcon::fromTheme("zoom-fit-width"));
    m_KStarsFOV->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    m_KStarsFOV->setMaximumSize(QSize(32, 32));
    m_KStarsFOV->setMinimumSize(QSize(32, 32));
    m_KStarsFOV->setToolTip(i18n("Zoom to the current KStars FOV.   Note: has no effect if hovering over object."));
    viewControlsLayout->addWidget(m_KStarsFOV);
    connect(m_KStarsFOV, SIGNAL(clicked()), this, SLOT(setKStarsXPlanetFOV()));

    m_setFOV = new QPushButton(this);
    m_setFOV->setIcon(QIcon::fromTheme("view-list-details"));
    m_setFOV->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    m_setFOV->setMaximumSize(QSize(32, 32));
    m_setFOV->setMinimumSize(QSize(32, 32));
    m_setFOV->setToolTip(i18n("Zoom to a specific FOV. This has no effect when hovering over an object"));
    viewControlsLayout->addWidget(m_setFOV);
    connect(m_setFOV, SIGNAL(clicked()), this, SLOT(setFOVfromList()));

    m_NoFOV = new QPushButton(this);
    m_NoFOV->setIcon(QIcon::fromTheme("system-reboot"));
    m_NoFOV->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    m_NoFOV->setMaximumSize(QSize(32, 32));
    m_NoFOV->setMinimumSize(QSize(32, 32));
    m_NoFOV->setToolTip(i18n("Optimum FOV for the target, FOV parameter not specified.  Note: has no effect if hovering over object."));
    viewControlsLayout->addWidget(m_NoFOV);
    connect(m_NoFOV, SIGNAL(clicked()), this, SLOT(resetXPlanetFOV()));

    m_Rotation = 0;

    viewControlsLayout->addWidget(new QLabel(i18n("Rotation:"), this));

    m_RotateEdit = new QSpinBox();

    m_RotateEdit->setRange(-180, 180);
    m_RotateEdit->setValue(0);
    m_RotateEdit->setSingleStep(10);
    m_RotateEdit->setToolTip(i18n("Set the view rotation to the desired angle"));
    viewControlsLayout->addWidget(m_RotateEdit);
    connect(m_RotateEdit,  SIGNAL(valueChanged(int)), this, SLOT(updateXPlanetRotationEdit()));

    QPushButton *invertRotation = new QPushButton(this);
    invertRotation->setIcon(QIcon::fromTheme("object-flip-vertical"));
    invertRotation->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    invertRotation->setMaximumSize(QSize(32, 32));
    invertRotation->setMinimumSize(QSize(32, 32));
    invertRotation->setToolTip(i18n("Rotate the view 180 degrees"));
    viewControlsLayout->addWidget(invertRotation);
    connect(invertRotation, SIGNAL(clicked()), this, SLOT(invertXPlanetRotation()));

    QPushButton *resetRotation = new QPushButton(this);
    resetRotation->setIcon(QIcon::fromTheme("system-reboot"));
    resetRotation->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetRotation->setMaximumSize(QSize(32, 32));
    resetRotation->setMinimumSize(QSize(32, 32));
    resetRotation->setToolTip(i18n("Reset view rotation to 0"));
    viewControlsLayout->addWidget(resetRotation);
    connect(resetRotation, SIGNAL(clicked()), this, SLOT(resetXPlanetRotation()));

    QPushButton *optionsB = new QPushButton(this);
    optionsB->setIcon(QIcon::fromTheme("configure"));
    optionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    optionsB->setMaximumSize(QSize(32, 32));
    optionsB->setMinimumSize(QSize(32, 32));
    optionsB->setToolTip(i18n("Bring up XPlanet Options"));
    viewControlsLayout->addWidget(optionsB);
    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));

    QPushButton *invertB = new QPushButton(this);
    invertB->setIcon(QIcon::fromTheme("edit-select-invert"));
    invertB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    invertB->setMaximumSize(QSize(32, 32));
    invertB->setMinimumSize(QSize(32, 32));
    invertB->setToolTip(i18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects "
                             "only the display and not the saving."));
    viewControlsLayout->addWidget(invertB);
    connect(invertB, SIGNAL(clicked()), this, SLOT(invertColors()));

    QWidget *timeWidget = new QWidget(this);
    QHBoxLayout *timeLayout = new QHBoxLayout(timeWidget);
    mainLayout->addWidget(timeWidget);
    timeLayout->setContentsMargins(0, 0, 0, 0);

    m_XPlanetTime = KStarsData::Instance()->lt();

    QPushButton *setTime = new QPushButton(this);
    setTime->setIcon(QIcon::fromTheme("clock"));
    setTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    setTime->setMaximumSize(QSize(32, 32));
    setTime->setMinimumSize(QSize(32, 32));
    setTime->setToolTip(i18n("Allows you to set the XPlanet time to a different date/time from KStars"));
    timeLayout->addWidget(setTime);
    connect(setTime, SIGNAL(clicked()), this, SLOT(setXPlanetTime()));

    QPushButton *kstarsTime = new QPushButton(this);
    kstarsTime->setIcon(QIcon::fromTheme("system-reboot"));
    kstarsTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    kstarsTime->setMaximumSize(QSize(32, 32));
    kstarsTime->setMinimumSize(QSize(32, 32));
    kstarsTime->setToolTip(i18n("Sets the XPlanet time to the current KStars time"));
    timeLayout->addWidget(kstarsTime);
    connect(kstarsTime, SIGNAL(clicked()), this, SLOT(setXPlanetTimetoKStarsTime()));

    m_XPlanetTimeDisplay = new QLabel(this);
    m_XPlanetTimeDisplay->setToolTip(i18n("Current XPlanet Time"));
    timeLayout->addWidget(m_XPlanetTimeDisplay);

    m_XPlanetTimeDisplay->setText(i18n("%1, %2", m_XPlanetTime.date().toString(), m_XPlanetTime.time().toString()));

    m_TimeSlider = new QSlider(Qt::Horizontal, this);
    m_TimeSlider->setTracking(false);
    connect(m_TimeSlider, SIGNAL(sliderMoved(int)), this, SLOT(timeSliderDisplay(int)));
    timeLayout->addWidget(m_TimeSlider);
    m_TimeSlider->setRange(-100, 100);
    m_TimeSlider->setToolTip(i18n("This sets the time step from the current XPlanet time, good for viewing events"));
    connect(m_TimeSlider,  SIGNAL(valueChanged(int)), this, SLOT(updateXPlanetTime(int)));

    m_TimeEdit = new QSpinBox(this);
    m_TimeEdit->setRange(-10000, 10000);
    m_TimeEdit->setMaximumWidth(50);
    m_TimeEdit->setToolTip(i18n("This sets the time step from the current XPlanet time"));
    timeLayout->addWidget(m_TimeEdit);
    connect(m_TimeEdit,  SIGNAL(valueChanged(int)), this, SLOT(updateXPlanetTimeEdit()));

    m_CurrentTimeUnitIndex = MINS;
    m_TimeUnitsSelect = new QComboBox(this);
    timeLayout->addWidget(m_TimeUnitsSelect);
    m_TimeUnitsSelect->addItem(i18n("years"));
    m_TimeUnitsSelect->addItem(i18n("months"));
    m_TimeUnitsSelect->addItem(i18n("days"));
    m_TimeUnitsSelect->addItem(i18n("hours"));
    m_TimeUnitsSelect->addItem(i18n("minutes"));
    m_TimeUnitsSelect->addItem(i18n("seconds"));
    m_TimeUnitsSelect->setCurrentIndex(MINS);
    m_TimeUnitsSelect->setToolTip(i18n("Lets you change the units for the timestep in the animation"));
    connect(m_TimeUnitsSelect,  SIGNAL(currentIndexChanged(int)), this, SLOT(updateXPlanetTimeUnits(int)));

    m_XPlanetTimer = new QTimer(this);
    m_XPlanetTimer->setInterval(Options::xplanetAnimationDelay());
    connect(m_XPlanetTimer, SIGNAL(timeout()), this, SLOT(incrementXPlanetTime()));

    m_RunTime = new QPushButton(this);
    m_RunTime->setIcon(QIcon::fromTheme("media-playback-start"));
    m_RunTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    m_RunTime->setCheckable(true);
    m_RunTime->setMaximumSize(QSize(32, 32));
    m_RunTime->setMinimumSize(QSize(32, 32));
    m_RunTime->setToolTip(i18n("Lets you run the animation"));
    timeLayout->addWidget(m_RunTime);
    connect(m_RunTime, SIGNAL(clicked()), this, SLOT(toggleXPlanetRun()));

    QPushButton *resetTime = new QPushButton(this);
    resetTime->setIcon(QIcon::fromTheme("system-reboot"));
    resetTime->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetTime->setMaximumSize(QSize(32, 32));
    resetTime->setMinimumSize(QSize(32, 32));
    resetTime->setToolTip(i18n("Resets the animation to 0 timesteps from the current XPlanet Time"));
    timeLayout->addWidget(resetTime);
    connect(resetTime, SIGNAL(clicked()), this, SLOT(resetXPlanetTime()));

    m_View = new XPlanetImageLabel(page);
    m_View->setAutoFillBackground(false);
    m_Caption = new QLabel(page);
    m_Caption->setAutoFillBackground(true);
    m_Caption->setFrameShape(QFrame::StyledPanel);
    m_Caption->setText(m_ObjectName);
    // Add layout
    QVBoxLayout *vlay = new QVBoxLayout(page);
    vlay->setSpacing(0);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->addWidget(m_View);
    vlay->addWidget(m_Caption);


    connect(m_View, SIGNAL(zoomIn()), this, SLOT(zoomInXPlanetFOV()));
    connect(m_View, SIGNAL(zoomOut()), this, SLOT(zoomOutXPlanetFOV()));
    connect(m_View, SIGNAL(changePosition(QPoint)), this, SLOT(changeXPlanetPosition(QPoint)));
    connect(m_View, SIGNAL(changeLocation(QPoint)), this, SLOT(changeXPlanetLocation(QPoint)));

    //Reverse colors
    QPalette p = palette();
    p.setColor(QPalette::Window, palette().color(QPalette::WindowText));
    p.setColor(QPalette::WindowText, palette().color(QPalette::Window));
    m_Caption->setPalette(p);
    m_View->setPalette(p);

#ifndef Q_OS_WIN
    if(Options::xplanetUseFIFO())
    {
        connect(&watcherTimeout, SIGNAL(timeout()), &fifoImageLoadWatcher, SLOT(cancel()));
        connect(&fifoImageLoadWatcher, SIGNAL(finished()), this, SLOT(showImage()));
    }
#endif


#ifdef Q_OS_OSX
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
#endif
    updateXPlanetTime(0);
#endif
}

XPlanetImageViewer::~XPlanetImageViewer()
{
    QApplication::restoreOverrideCursor();
}

void XPlanetImageViewer::startXplanet()
{
    if(m_XPlanetRunning)
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
        KSNotification::error(i18n("Xplanet binary path is empty in config panel."));
        return;
    }

    // If Options::xplanetPath() does not exist, return
    const QFileInfo xPlanetLocationInfo(xPlanetLocation);
    if (!xPlanetLocationInfo.exists() || !xPlanetLocationInfo.isExecutable())
    {
        KSNotification::error(i18n("The configured Xplanet binary does not exist or is not executable."));
        return;
    }

    // Create xplanet process
    QProcess *xplanetProc = new QProcess(this);

    // Add some options
    QStringList args;

    //This specifies the object to be viewed
    args << "-body" << m_ObjectName.toLower();
    //This is the date and time requested
    args << "-date" << m_Date;
    //This is the glare from the sun
    args << "-glare" << Options::xplanetGlare();
    args << "-base_magnitude" << Options::xplanetMagnitude();
    //This is the correction for light's travel time.
    args << "-light_time";

    args << "-geometry" << QString::number(Options::xplanetWidth()) + 'x' + QString::number(Options::xplanetHeight());

    if(m_FOV != 0)
        args << "-fov" << QString::number(m_FOV);
    //Need to convert to locale for places that don't use decimals??
    //args << "-fov" << fov.setNum(fov());//.replace('.', ',');

    //This rotates the view for different object angles
    args << "-rotate" << QString::number(m_Rotation);

    if (Options::xplanetConfigFile())
        args << "-config" << Options::xplanetConfigFilePath();
    if (Options::xplanetStarmap())
        args << "-starmap" << Options::xplanetStarmapPath();
    if (Options::xplanetArcFile())
        args << "-arc_file" << Options::xplanetArcFilePath();
    if (!m_File.fileName().isEmpty())
        args << "-output" << m_File.fileName() << "-quality" << Options::xplanetQuality();

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

    if(m_CurrentObjectIndex == m_CurrentOriginIndex)
    {
        if (Options::xplanetRandom())
            args << "-random";
        else
            args << "-latitude" << QString::number(m_lat) << "-longitude" << QString::number(m_lon) << "-radius" << QString::number(m_Radius);
    }
    else
        args << "-origin" << m_OriginName;

    //Centering
    //This allows you to recenter the xplanet view

    args << "-center" << "+" + QString::number(Options::xplanetWidth() / 2 + center.x()) + "+" + QString::number(Options::xplanetHeight() / 2 + center.y());

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

#ifdef Q_OS_WIN
    QString searchDir = xPlanetLocationInfo.dir().absolutePath() + QDir::separator() + "xplanet";
    args << "-searchdir" << searchDir;
#endif

    //This prevents it from running forever.
    args << "-num_times" << "1";

    m_XPlanetRunning = true;
    m_ImageLoadSucceeded = false; //This will be set to true if it works.
    uint32_t timeout = Options::xplanetTimeout();

    //FIFO files don't work on windows
#ifndef Q_OS_WIN
    if(Options::xplanetUseFIFO())
    {
        fifoImageLoadWatcher.setFuture(QtConcurrent::run(this, &XPlanetImageViewer::loadImage));
        watcherTimeout.setSingleShot(true);
        watcherTimeout.start(timeout);
    }
#endif

    xplanetProc->start(xPlanetLocation, args);

    //Uncomment to print the XPlanet commands to the console
    // qDebug() << "Run:" << xplanetProc->program() << args.join(" ");

    bool XPlanetSucceeded = xplanetProc->waitForFinished(timeout);
    m_XPlanetRunning = false;
    xplanetProc->kill(); //In case it timed out
    xplanetProc->deleteLater();
    if(XPlanetSucceeded)
    {
        if(m_FOV == 0)
            m_Caption->setText(i18n("XPlanet View: %1 from %2 on %3", m_ObjectName, m_OriginName, m_DateText));
        else
            m_Caption->setText(i18n("XPlanet View: %1 from %2 on %3 at FOV: %4 deg", m_ObjectName, m_OriginName, m_DateText, m_FOV));
#ifdef Q_OS_WIN
        loadImage(); //This will also set imageLoadSucceeded based on whether it worked.
#else
        if(Options::xplanetUseFIFO())
            return;  //The loading of the image is handled with the watcher
        else
            loadImage(); //This will also set imageLoadSucceeded based on whether it worked.
#endif

        if(m_ImageLoadSucceeded)
            showImage();
        else
        {
            KSNotification::error(i18n("Loading of the image of object %1 failed.", m_ObjectName));
        }
    }
    else
    {
        KStars::Instance()->statusBar()->showMessage(i18n("XPlanet failed to generate the image for object %1 before the timeout expired.", m_ObjectName));
#ifndef Q_OS_WIN
        if(Options::xplanetUseFIFO())
            fifoImageLoadWatcher.cancel();
#endif
    }
}

bool XPlanetImageViewer::setupOutputFile()
{
#ifndef Q_OS_WIN
    if(Options::xplanetUseFIFO())
    {
        if(m_File.fileName().contains("xplanetfifo") && m_File.exists())
            return true;
        QDir kstarsTempDir(KSPaths::writableLocation(QStandardPaths::TempLocation) + "/" + qAppName());
        kstarsTempDir.mkpath(".");
        m_File.setFileName(kstarsTempDir.filePath(QString("xplanetfifo%1.png").arg(QUuid::createUuid().toString().mid(1, 8)).toLatin1()));
        int mkFifoSuccess = 0; //Note if the return value of the command is 0 it succeeded, -1 means it failed.
        if ((mkFifoSuccess = mkfifo(m_File.fileName().toLatin1(), S_IRUSR | S_IWUSR) < 0))
        {
            KSNotification::error(i18n("Error making FIFO file %1: %2.", m_File.fileName(), strerror(errno)));
            return false;
        }
        return true;
    }
#endif

    //If the user is using windows or has not selected to use FIFO, it uses files in the KStars data directory.
    QDir xPlanetDirPath(KSPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + "xplanet");
    xPlanetDirPath.mkpath(".");
    m_File.setFileName(xPlanetDirPath.filePath(m_ObjectName + ".png"));
    return true;
}

void XPlanetImageViewer::zoomInXPlanetFOV()
{
    if(m_CurrentObjectIndex == m_CurrentOriginIndex)
    {
        m_Radius += 5;
        updatePositionDisplay();
        startXplanet();
    }
    else
    {
        m_FOVEdit->stepDown();
        startXplanet();
    }

}

void XPlanetImageViewer::zoomOutXPlanetFOV()
{
    if(m_CurrentObjectIndex == m_CurrentOriginIndex)
    {
        if(m_Radius > 0)
        {
            m_Radius -= 5;
            updatePositionDisplay();
            startXplanet();
        }
    }
    else
    {
        m_FOVEdit->stepUp();
        startXplanet();
    }

}

void XPlanetImageViewer::updatePositionDisplay()
{
    m_PositionDisplay->setText(i18n("%1, %2, %3", QString::number(m_lat), QString::number(m_lon), QString::number(m_Radius)));
}

void XPlanetImageViewer::updateXPlanetTime(int timeShift)
{

    KStarsDateTime shiftedXPlanetTime;
    switch(m_CurrentTimeUnitIndex)
    {
        case YEARS:
            shiftedXPlanetTime = m_XPlanetTime.addDays(timeShift * 365);
            break;

        case MONTHS:
            shiftedXPlanetTime = m_XPlanetTime.addDays(timeShift * 30);
            break;

        case DAYS:
            shiftedXPlanetTime = m_XPlanetTime.addDays(timeShift);
            break;

        case HOURS:
            shiftedXPlanetTime = m_XPlanetTime.addSecs(timeShift * 3600);
            break;

        case MINS:
            shiftedXPlanetTime = m_XPlanetTime.addSecs(timeShift * 60);
            break;

        case SECS:
            shiftedXPlanetTime = m_XPlanetTime.addSecs(timeShift);
            break;
    }

    setXPlanetDate(shiftedXPlanetTime);
    m_DateText = i18n("%1, %2", shiftedXPlanetTime.date().toString(), shiftedXPlanetTime.time().toString());
    if(m_TimeEdit->value() != timeShift)
        m_TimeEdit->setValue(timeShift);
    else
        startXplanet();
}

void XPlanetImageViewer::updateXPlanetObject(int objectIndex)
{
    center = QPoint(0, 0);
    m_CurrentObjectIndex = objectIndex;
    m_ObjectName = m_ObjectNames.at(objectIndex);

    setWindowTitle(i18nc("@title:window", "XPlanet Solar System Simulator: %1", m_ObjectName));
    if(m_FreeRotate->isChecked())
        m_OriginSelector->setCurrentIndex(m_CurrentObjectIndex);
    if(m_CurrentObjectIndex == m_CurrentOriginIndex)
        startXplanet();
    else
        resetXPlanetFOV();
}

void XPlanetImageViewer::updateXPlanetOrigin(int originIndex)
{
    center = QPoint(0, 0);
    m_CurrentOriginIndex = originIndex;
    m_OriginName = m_ObjectNames.at(originIndex);
    if(m_CurrentObjectIndex == m_CurrentOriginIndex)
        m_FreeRotate->setChecked(true);
    else
        m_FreeRotate->setChecked(false);
    updateStates();//This will update the disabled/enabled states
    startXplanet();
}

void XPlanetImageViewer::changeXPlanetLocation(QPoint delta)
{
    if(m_CurrentObjectIndex == m_CurrentOriginIndex)
    {
        double newLon = m_lon + delta.x();
        double newLat = m_lat + delta.y();

        newLat = qBound(-90.0, newLat, 90.0);

        m_lon = newLon;
        m_lat = newLat;

        updatePositionDisplay();
        startXplanet();
    }
}

void XPlanetImageViewer::changeXPlanetPosition(QPoint delta)
{
    center.setX(center.x() + delta.x());
    center.setY(center.y() + delta.y());
    startXplanet();
}

void XPlanetImageViewer::reCenterXPlanet()
{
    center = QPoint(0, 0);
    startXplanet();
}

void XPlanetImageViewer::resetLocation()
{
    m_lat = Options::xplanetLatitude().toDouble();
    m_lon = Options::xplanetLongitude().toDouble();
    m_Radius = 45;
    updatePositionDisplay();
    startXplanet();
}

void XPlanetImageViewer::slotFreeRotate()
{
    if(m_FreeRotate->isChecked())
        m_OriginSelector->setCurrentIndex(m_CurrentObjectIndex);
    else
        m_OriginSelector->setCurrentIndex(EARTH);
}

void XPlanetImageViewer::updateStates()
{
    if(m_FreeRotate->isChecked())
    {
        m_FOVEdit->setDisabled(true);
        m_KStarsFOV->setDisabled(true);
        m_NoFOV->setDisabled(true);

        m_PositionDisplay->setDisabled(false);
    }
    else
    {
        m_FOVEdit->setDisabled(false);
        m_KStarsFOV->setDisabled(false);
        m_NoFOV->setDisabled(false);

        m_PositionDisplay->setDisabled(true);
    }
}

void XPlanetImageViewer::setXPlanetDate(KStarsDateTime time)
{
    //Note Xplanet uses UT time for everything but we want the labels to all be LT
    KStarsDateTime utTime = KStarsData::Instance()->geo()->LTtoUT(time);
    m_Date = utTime.toString(Qt::ISODate)
             .replace("-", QString(""))
             .replace("T", ".")
             .replace(":", QString(""))
             .replace("Z", QString(""));
}

void XPlanetImageViewer::updateXPlanetTimeUnits(int units)
{
    m_CurrentTimeUnitIndex = units;
    updateXPlanetTimeEdit();
}

void XPlanetImageViewer::updateXPlanetTimeEdit()
{
    if(m_TimeSlider->isSliderDown())
        return;
    int timeShift = m_TimeEdit->value();
    if(m_TimeSlider->value() != timeShift)
    {

        if(timeShift > m_TimeSlider->maximum() + 100)
            m_TimeSlider->setMaximum(timeShift);
        if(timeShift < m_TimeSlider->minimum() - 100)
            m_TimeSlider->setMinimum(timeShift);
        m_TimeSlider->setValue(timeShift);
    }
    else
        updateXPlanetTime(timeShift);
}

void XPlanetImageViewer::timeSliderDisplay(int timeShift)
{
    m_TimeEdit->setValue(timeShift);
}

void XPlanetImageViewer::incrementXPlanetTime()
{
    if(!m_XPlanetRunning)
    {
        int timeShift = m_TimeEdit->value();
        if(m_TimeSlider->maximum() <= timeShift)
            m_TimeSlider->setMaximum(timeShift + 100);
        if(m_TimeEdit->maximum() <= timeShift)
            m_TimeEdit->setMaximum(timeShift + 100);
        m_TimeSlider->setValue(timeShift + 1);
    }
}

void XPlanetImageViewer::setXPlanetTime()
{
    QPointer<TimeDialog> timedialog = new TimeDialog(m_XPlanetTime, KStarsData::Instance()->geo(), this);
    if (timedialog->exec() == QDialog::Accepted)
    {
        m_XPlanetTime = timedialog->selectedDateTime();
        m_XPlanetTimeDisplay->setText(i18n("%1, %2", m_XPlanetTime.date().toString(), m_XPlanetTime.time().toString()));
        int timeShift = 0;
        m_TimeSlider->setRange(-100, 100);
        if(m_TimeSlider->value() != timeShift)
            m_TimeSlider->setValue(timeShift);
        else
            updateXPlanetTime(timeShift);
    }
}

void XPlanetImageViewer::setXPlanetTimetoKStarsTime()
{
    m_XPlanetTime = KStarsData::Instance()->lt();
    m_XPlanetTimeDisplay->setText(i18n("%1, %2", m_XPlanetTime.date().toString(), m_XPlanetTime.time().toString()));
    int timeShift = 0;
    m_TimeSlider->setRange(-100, 100);
    if(m_TimeSlider->value() != timeShift)
        m_TimeSlider->setValue(timeShift);
    else
        updateXPlanetTime(timeShift);
}

void XPlanetImageViewer::resetXPlanetTime()
{
    int timeShift = 0;
    m_TimeSlider->setRange(-100, 100);
    if(m_TimeSlider->value() != timeShift)
        m_TimeSlider->setValue(timeShift);
    else
        updateXPlanetTime(timeShift);
}

void XPlanetImageViewer::toggleXPlanetRun()
{
    if(m_XPlanetTimer->isActive())
    {
        m_XPlanetTimer->stop();
#ifndef Q_OS_WIN
        if(Options::xplanetUseFIFO())
            fifoImageLoadWatcher.cancel();
#endif
    }
    else
    {
        m_XPlanetTimer->setInterval(Options::xplanetAnimationDelay());
        m_XPlanetTimer->start();
    }
}

void XPlanetImageViewer::updateXPlanetFOVEdit()
{
    m_FOV = m_FOVEdit->value();
    startXplanet();
}

void XPlanetImageViewer::resetXPlanetFOV()
{
    m_FOV = m_objectDefaultFOVs.at(m_CurrentObjectIndex);
    m_FOVEdit->setValue(m_FOV);
    startXplanet();
}

void XPlanetImageViewer::setKStarsXPlanetFOV()
{
    m_FOV = KStars::Instance()->map()->fov();
    m_FOVEdit->setValue(m_FOV);
    startXplanet();
}
void XPlanetImageViewer::setFOVfromList()
{
    if (!KStarsData::Instance()->getAvailableFOVs().isEmpty())
    {
        // Ask the user to choose from a list of available FOVs.
        QMap<QString, const FOV *> nameToFovMap;
        for (const FOV *f : KStarsData::Instance()->getAvailableFOVs())
        {
            nameToFovMap.insert(f->name(), f);
        }
        bool ok = false;
        const FOV *fov = nullptr;
        fov = nameToFovMap[QInputDialog::getItem(this, i18n("Choose a field-of-view"),
                                                       i18n("FOV to render in XPlanet:"), nameToFovMap.uniqueKeys(), 0,
                                                       false, &ok)];
        if (ok)
        {
            m_FOV = fov->sizeX() / 60 ; //Converting from arcmin to degrees
            m_FOVEdit->setValue(m_FOV);
            startXplanet();
        }
    }
}

void XPlanetImageViewer::updateXPlanetRotationEdit()
{
    m_Rotation = m_RotateEdit->value();
    startXplanet();
}

void XPlanetImageViewer::resetXPlanetRotation()
{
    m_RotateEdit->setValue(0);
}

void XPlanetImageViewer::invertXPlanetRotation()
{
    m_RotateEdit->setValue(180);
}

bool XPlanetImageViewer::loadImage()
{
#ifndef KSTARS_LITE

    if (!m_Image.load(m_File.fileName()))
    {
        m_ImageLoadSucceeded = false;
        return false;
    }
    m_ImageLoadSucceeded = true;
    return true;
#else
    imageLoadSucceeded = false;
    return false;
#endif
}

bool XPlanetImageViewer::showImage()
{
#ifndef KSTARS_LITE

    //If the image is larger than screen width and/or screen height,
    //shrink it to fit the screen
    QRect deskRect = QGuiApplication::primaryScreen()->geometry();
    int w          = deskRect.width();  // screen width
    int h          = deskRect.height(); // screen height

    if (m_Image.width() <= w && m_Image.height() > h) //Window is taller than desktop
        m_Image = m_Image.scaled(int(m_Image.width() * h / m_Image.height()), h);
    else if (m_Image.height() <= h && m_Image.width() > w) //window is wider than desktop
        m_Image = m_Image.scaled(w, int(m_Image.height() * w / m_Image.width()));
    else if (m_Image.width() > w && m_Image.height() > h) //window is too tall and too wide
    {
        //which needs to be shrunk least, width or height?
        float fx = float(w) / float(m_Image.width());
        float fy = float(h) / float(m_Image.height());
        if (fx > fy) //width needs to be shrunk less, so shrink to fit in height
            m_Image = m_Image.scaled(int(m_Image.width() * fy), h);
        else //vice versa
            m_Image = m_Image.scaled(w, int(m_Image.height() * fx));
    }
    const bool initialLoad = !isVisible();

    show(); // hide is default

    m_View->setImage(m_Image);
    w = m_Image.width();

    //If the caption is wider than the image, set the window size
    //to fit the caption
    if (m_Caption->width() > w)
        w = m_Caption->width();
    if(initialLoad)
        resize(w, m_Image.height());
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
    QFileDialog saveDialog(KStars::Instance(), i18nc("@title:window", "Save Image"), m_LastFile);
    saveDialog.setDefaultSuffix("png");
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.exec();

    if(saveDialog.result() == QFileDialog::Rejected)
        return;
    if(saveDialog.selectedFiles().isEmpty())
        return;
    QString newFileName = saveDialog.selectedFiles().first();
    if(newFileName.isEmpty())
        return;

    m_LastFile = newFileName;

    saveFile(newFileName);

#endif
}

void XPlanetImageViewer::saveFile(const QString &fileName)
{
#ifndef KSTARS_LITE

    if (! m_Image.save(fileName, "png"))
    {
        KSNotification::error(i18n("Saving of the image to %1 failed.", fileName));
    }
    else
        KStars::Instance()->statusBar()->showMessage(i18n("Saved image to %1", fileName));
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

