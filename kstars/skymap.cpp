/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjectuserdata.h"
#ifdef _WIN32
#include <windows.h>
#endif

#include "skymap.h"

#include "ksasteroid.h"
#include "kstars_debug.h"
#include "fov.h"
#include "imageviewer.h"
#include "xplanetimageviewer.h"
#include "ksdssdownloader.h"
#include "kspaths.h"
#include "kspopupmenu.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#include "skymapcomposite.h"
#ifdef HAVE_OPENGL
#include "skymapgldraw.h"
#endif
#include "skymapqdraw.h"
#include "starhopperdialog.h"
#include "starobject.h"
#include "texturemanager.h"
#include "dialogs/detaildialog.h"
#include "printing/printingwizard.h"
#include "skycomponents/flagcomponent.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/satellite.h"
#include "tools/flagmanager.h"
#include "widgets/infoboxwidget.h"
#include "projections/azimuthalequidistantprojector.h"
#include "projections/equirectangularprojector.h"
#include "projections/lambertprojector.h"
#include "projections/gnomonicprojector.h"
#include "projections/orthographicprojector.h"
#include "projections/stereographicprojector.h"
#include "catalogobject.h"
#include "catalogsdb.h"
#include "catalogscomponent.h"

#include <KActionCollection>
#include <KToolBar>

#include <QBitmap>
#include <QToolTip>
#include <QClipboard>
#include <QInputDialog>
#include <QDesktopServices>

#include <QProcess>
#include <QFileDialog>

#include <cmath>

namespace
{
// Draw bitmap for zoom cursor. Width is size of pen to draw with.
QBitmap zoomCursorBitmap(int width)
{
    QBitmap b(32, 32);
    b.fill(Qt::color0);
    int mx = 16, my = 16;
    // Begin drawing
    QPainter p;
    p.begin(&b);
    p.setPen(QPen(Qt::color1, width));
    p.drawEllipse(mx - 7, my - 7, 14, 14);
    p.drawLine(mx + 5, my + 5, mx + 11, my + 11);
    p.end();
    return b;
}

// Draw bitmap for default cursor. Width is size of pen to draw with.
QBitmap defaultCursorBitmap(int width)
{
    QBitmap b(32, 32);
    b.fill(Qt::color0);
    int mx = 16, my = 16;
    // Begin drawing
    QPainter p;
    p.begin(&b);
    p.setPen(QPen(Qt::color1, width));
    // 1. diagonal
    p.drawLine(mx - 2, my - 2, mx - 8, mx - 8);
    p.drawLine(mx + 2, my + 2, mx + 8, mx + 8);
    // 2. diagonal
    p.drawLine(mx - 2, my + 2, mx - 8, mx + 8);
    p.drawLine(mx + 2, my - 2, mx + 8, mx - 8);
    p.end();
    return b;
}

QBitmap circleCursorBitmap(int width)
{
    QBitmap b(32, 32);
    b.fill(Qt::color0);
    int mx = 16, my = 16;
    // Begin drawing
    QPainter p;
    p.begin(&b);
    p.setPen(QPen(Qt::color1, width));

    // Circle
    p.drawEllipse(mx - 8, my - 8, mx, my);
    // 1. diagonal
    p.drawLine(mx - 8, my - 8, 0, 0);
    p.drawLine(mx + 8, my - 8, 32, 0);
    // 2. diagonal
    p.drawLine(mx - 8, my + 8, 0, 32);
    p.drawLine(mx + 8, my + 8, 32, 32);

    p.end();
    return b;
}

} // namespace

SkyMap *SkyMap::pinstance = nullptr;

SkyMap *SkyMap::Create()
{
    delete pinstance;
    pinstance = new SkyMap();
    return pinstance;
}

SkyMap *SkyMap::Instance()
{
    return pinstance;
}

SkyMap::SkyMap()
    : QGraphicsView(KStars::Instance()), computeSkymap(true), rulerMode(false), data(KStarsData::Instance()), pmenu(nullptr),
      ClickedObject(nullptr), FocusObject(nullptr), m_proj(nullptr), m_previewLegend(false), m_objPointingMode(false)
{
#if !defined(KSTARS_LITE)
    grabGesture(Qt::PinchGesture);
    grabGesture(Qt::TapAndHoldGesture);
#endif
    m_Scale = 1.0;

    ZoomRect = QRect();

    // set the default cursor
    setMouseCursorShape(static_cast<Cursor>(Options::defaultCursor()));

    QPalette p = palette();
    p.setColor(QPalette::Window, QColor(data->colorScheme()->colorNamed("SkyColor")));
    setPalette(p);

    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(380, 250);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QGraphicsView { border-style: none; }");

    setMouseTracking(true); //Generate MouseMove events!
    midMouseButtonDown = false;
    mouseButtonDown    = false;
    slewing            = false;
    clockSlewing       = false;

    ClickedObject = nullptr;
    FocusObject   = nullptr;

    m_SkyMapDraw = nullptr;

    pmenu = new KSPopupMenu();

    setupProjector();

    //Initialize Transient label stuff
    m_HoverTimer.setSingleShot(true); // using this timer as a single shot timer

    connect(&m_HoverTimer, SIGNAL(timeout()), this, SLOT(slotTransientLabel()));
    connect(this, SIGNAL(destinationChanged()), this, SLOT(slewFocus()));
    connect(KStarsData::Instance(), SIGNAL(skyUpdate(bool)), this, SLOT(slotUpdateSky(bool)));

    // Time infobox
    m_timeBox = new InfoBoxWidget(Options::shadeTimeBox(), Options::positionTimeBox(), Options::stickyTimeBox(),
                                  QStringList(), this);
    m_timeBox->setVisible(Options::showTimeBox());
    connect(data->clock(), SIGNAL(timeChanged()), m_timeBox, SLOT(slotTimeChanged()));
    connect(data->clock(), SIGNAL(timeAdvanced()), m_timeBox, SLOT(slotTimeChanged()));

    // Geo infobox
    m_geoBox = new InfoBoxWidget(Options::shadeGeoBox(), Options::positionGeoBox(), Options::stickyGeoBox(),
                                 QStringList(), this);
    m_geoBox->setVisible(Options::showGeoBox());
    connect(data, SIGNAL(geoChanged()), m_geoBox, SLOT(slotGeoChanged()));

    // Object infobox
    m_objBox = new InfoBoxWidget(Options::shadeFocusBox(), Options::positionFocusBox(), Options::stickyFocusBox(),
                                 QStringList(), this);
    m_objBox->setVisible(Options::showFocusBox());
    connect(this, SIGNAL(objectChanged(SkyObject*)), m_objBox, SLOT(slotObjectChanged(SkyObject*)));
    connect(this, SIGNAL(positionChanged(SkyPoint*)), m_objBox, SLOT(slotPointChanged(SkyPoint*)));

    m_SkyMapDraw = new SkyMapQDraw(this);
    m_SkyMapDraw->setMouseTracking(true);

    m_SkyMapDraw->setParent(this->viewport());
    m_SkyMapDraw->show();

    m_iboxes = new InfoBoxes(m_SkyMapDraw);

    m_iboxes->setVisible(Options::showInfoBoxes());
    m_iboxes->addInfoBox(m_timeBox);
    m_iboxes->addInfoBox(m_geoBox);
    m_iboxes->addInfoBox(m_objBox);
}

void SkyMap::slotToggleGeoBox(bool flag)
{
    m_geoBox->setVisible(flag);
}

void SkyMap::slotToggleFocusBox(bool flag)
{
    m_objBox->setVisible(flag);
}

void SkyMap::slotToggleTimeBox(bool flag)
{
    m_timeBox->setVisible(flag);
}

void SkyMap::slotToggleInfoboxes(bool flag)
{
    m_iboxes->setVisible(flag);
    Options::setShowInfoBoxes(flag);
}

SkyMap::~SkyMap()
{
    /* == Save infoxes status into Options == */
    //Options::setShowInfoBoxes(m_iboxes->isVisibleTo(parentWidget()));
    // Time box
    Options::setPositionTimeBox(m_timeBox->pos());
    Options::setShadeTimeBox(m_timeBox->shaded());
    Options::setStickyTimeBox(m_timeBox->sticky());
    Options::setShowTimeBox(m_timeBox->isVisibleTo(m_iboxes));
    // Geo box
    Options::setPositionGeoBox(m_geoBox->pos());
    Options::setShadeGeoBox(m_geoBox->shaded());
    Options::setStickyGeoBox(m_geoBox->sticky());
    Options::setShowGeoBox(m_geoBox->isVisibleTo(m_iboxes));
    // Obj box
    Options::setPositionFocusBox(m_objBox->pos());
    Options::setShadeFocusBox(m_objBox->shaded());
    Options::setStickyFocusBox(m_objBox->sticky());
    Options::setShowFocusBox(m_objBox->isVisibleTo(m_iboxes));

    //store focus values in Options
    //If not tracking and using Alt/Az coords, stor the Alt/Az coordinates
    if (Options::useAltAz() && !Options::isTracking())
    {
        Options::setFocusRA(focus()->az().Degrees());
        Options::setFocusDec(focus()->alt().Degrees());
    }
    else
    {
        Options::setFocusRA(focus()->ra().Hours());
        Options::setFocusDec(focus()->dec().Degrees());
    }

#ifdef HAVE_OPENGL
    delete m_SkyMapGLDraw;
    delete m_SkyMapQDraw;
    m_SkyMapDraw = 0; // Just a formality
#else
    delete m_SkyMapDraw;
#endif

    delete pmenu;

    delete m_proj;

    pinstance = nullptr;
}

void SkyMap::showFocusCoords()
{
    if (focusObject() && Options::isTracking())
        emit objectChanged(focusObject());
    else
        emit positionChanged(focus());
}

void SkyMap::slotTransientLabel()
{
    //This function is only called if the HoverTimer manages to timeout.
    //(HoverTimer is restarted with every mouseMoveEvent; so if it times
    //out, that means there was no mouse movement for HOVER_INTERVAL msec.)
    if (hasFocus() && !slewing &&
            !(Options::useAltAz() && Options::showGround() && m_MousePoint.altRefracted().Degrees() < 0.0))
    {
        double maxrad = 1000.0 / Options::zoomFactor();
        SkyObject *so = data->skyComposite()->objectNearest(&m_MousePoint, maxrad);

        if (so && !isObjectLabeled(so))
        {
            QToolTip::showText(QCursor::pos(),
                               i18n("%1: %2<sup>m</sup>", so->translatedLongName(), QString::number(so->mag(), 'f', 1)),
                               this);
        }
    }
}

//Slots

void SkyMap::setClickedObject(SkyObject *o)
{
    ClickedObject = o;
}

void SkyMap::setFocusObject(SkyObject *o)
{
    FocusObject = o;
    if (FocusObject)
        Options::setFocusObject(FocusObject->name());
    else
        Options::setFocusObject(i18n("nothing"));
}

void SkyMap::slotCenter()
{
    KStars *kstars        = KStars::Instance();
    TrailObject *trailObj = dynamic_cast<TrailObject *>(focusObject());

    SkyPoint *foc;
    if(ClickedObject != nullptr)
        foc = ClickedObject;
    else
        foc = &ClickedPoint;

    if (Options::useAltAz())
    {
        // JM 2016-09-12: Following call has problems when ra0/dec0 of an object are not valid for example
        // because they're solar system bodies. So it creates a lot of issues. It is disabled and centering
        // works correctly for all different body types as I tested.
        //DeepSkyObject *dso = dynamic_cast<DeepSkyObject *>(focusObject());
        //if (dso)
        //    foc->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);

        // JM 2018-05-06: No need to do the above
        foc->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
    else
        foc->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);

    qCDebug(KSTARS) << "Centering on " << foc->ra().toHMSString() << foc->dec().toDMSString();

    //clear the planet trail of old focusObject, if it was temporary
    if (trailObj && data->temporaryTrail)
    {
        trailObj->clearTrail();
        data->temporaryTrail = false;
    }

    //If the requested object is below the opaque horizon, issue a warning message
    //(unless user is already pointed below the horizon)
    if (Options::useAltAz() && Options::showGround() &&
            focus()->alt().Degrees() > SkyPoint::altCrit &&
            foc->alt().Degrees() <= SkyPoint::altCrit)
    {
        QString caption = i18n("Requested Position Below Horizon");
        QString message = i18n("The requested position is below the horizon.\nWould you like to go there anyway?");
        if (KMessageBox::warningYesNo(this, message, caption, KGuiItem(i18n("Go Anyway")),
                                      KGuiItem(i18n("Keep Position")), "dag_focus_below_horiz") == KMessageBox::No)
        {
            setClickedObject(nullptr);
            setFocusObject(nullptr);
            Options::setIsTracking(false);

            return;
        }
    }

    //set FocusObject before slewing.  Otherwise, KStarsData::updateTime() can reset
    //destination to previous object...
    setFocusObject(ClickedObject);
    if(ClickedObject == nullptr)
        setFocusPoint(&ClickedPoint);

    Options::setIsTracking(true);

    if (kstars)
    {
        kstars->actionCollection()
        ->action("track_object")
        ->setIcon(QIcon::fromTheme("document-encrypt"));
        kstars->actionCollection()->action("track_object")->setText(i18n("Stop &Tracking"));
    }

    //If focusObject is a SS body and doesn't already have a trail, set the temporaryTrail

    if (Options::useAutoTrail() && trailObj && trailObj->hasTrail())
    {
        trailObj->addToTrail();
        data->temporaryTrail = true;
    }

    //update the destination to the selected coordinates
    if (Options::useAltAz())
    {
        setDestinationAltAz(foc->alt(), foc->az(), false);
    }
    else
    {
        setDestination(*foc);
    }

    foc->EquatorialToHorizontal(data->lst(), data->geo()->lat());

    //display coordinates in statusBar
    emit mousePointChanged(foc);
    showFocusCoords(); //update FocusBox
}

void SkyMap::slotUpdateSky(bool now)
{
    // Code moved from KStarsData::updateTime()
    //Update focus
    updateFocus();

    if (now)
        QTimer::singleShot(
            0, this,
            SLOT(forceUpdateNow())); // Why is it done this way rather than just calling forceUpdateNow()? -- asimha // --> Opening a neww thread? -- Valentin
    else
        forceUpdate();
}

void SkyMap::slotDSS()
{
    dms ra(0.0), dec(0.0);
    QString urlstring;

    //ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
    //if we clicked on empty sky, we need to precess to J2000.
    if (clickedObject())
    {
        urlstring = KSDssDownloader::getDSSURL(clickedObject());
    }
    else
    {
        //SkyPoint deprecessedPoint = clickedPoint()->deprecess(data->updateNum());
        SkyPoint deprecessedPoint = clickedPoint()->catalogueCoord(data->updateNum()->julianDay());
        ra                        = deprecessedPoint.ra();
        dec                       = deprecessedPoint.dec();
        urlstring                 = KSDssDownloader::getDSSURL(ra, dec); // Use default size for non-objects
    }

    QUrl url(urlstring);

    KStars *kstars = KStars::Instance();
    if (kstars)
    {
        new ImageViewer(
            url, i18n("Digitized Sky Survey image provided by the Space Telescope Science Institute [free for non-commercial use]."),
            this);
        //iv->show();
    }
}

void SkyMap::slotCopyCoordinates()
{
    dms J2000RA(0.0), J2000DE(0.0), JNowRA(0.0), JNowDE(0.0), Az, Alt;
    if (clickedObject())
    {
        J2000RA  = clickedObject()->ra0();
        J2000DE = clickedObject()->dec0();
        JNowRA = clickedObject()->ra();
        JNowDE = clickedObject()->dec();
        Az = clickedObject()->az();
        Alt = clickedObject()->alt();
    }
    else
    {
        // Empty point only have valid JNow RA/DE, not J2000 information.
        SkyPoint emptyPoint = *clickedPoint();
        // Now get J2000 from JNow but de-aberrating, de-nutating, de-preccessing
        // This modifies emptyPoint, but the RA/DE are now missing and need
        // to be repopulated.
        emptyPoint.catalogueCoord(data->updateNum()->julianDay());
        emptyPoint.setRA(clickedPoint()->ra());
        emptyPoint.setDec(clickedPoint()->dec());
        emptyPoint.EquatorialToHorizontal(data->lst(), data->geo()->lat());

        J2000RA = emptyPoint.ra0();
        J2000DE = emptyPoint.dec0();
        JNowRA = emptyPoint.ra();
        JNowDE = emptyPoint.dec();
        Az = emptyPoint.az();
        Alt = emptyPoint.alt();
    }

    QApplication::clipboard()->setText(i18nc("Equatorial & Horizontal Coordinates",
                                       "JNow:\t%1\t%2\nJ2000:\t%3\t%4\nAzAlt:\t%5\t%6",
                                       JNowRA.toHMSString(),
                                       JNowDE.toDMSString(),
                                       J2000RA.toHMSString(),
                                       J2000DE.toDMSString(),
                                       Az.toDMSString(),
                                       Alt.toDMSString()));
}


void SkyMap::slotCopyTLE()
{

    QString tle = "";
    if (clickedObject()->type() == SkyObject::SATELLITE)
    {
        Satellite *sat = (Satellite *) clickedObject();
        tle = sat->tle();
    }
    else
    {
        tle = "NO TLE FOR OBJECT";
    }


    QApplication::clipboard()->setText(tle);
}

void SkyMap::slotSDSS()
{
    // TODO: Remove code duplication -- we have the same stuff
    // implemented in ObservingList::setCurrentImage() etc. in
    // tools/observinglist.cpp; must try to de-duplicate as much as
    // possible.
    QString URLprefix("http://skyserver.sdss.org/dr16/SkyServerWS/ImgCutout/getjpeg?");
    QString URLsuffix("&scale=1.0&width=600&height=600");
    dms ra(0.0), dec(0.0);
    QString RAString, DecString;

    //ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
    //if we clicked on empty sky, we need to precess to J2000.
    if (clickedObject())
    {
        ra  = clickedObject()->ra0();
        dec = clickedObject()->dec0();
    }
    else
    {
        //SkyPoint deprecessedPoint = clickedPoint()->deprecess(data->updateNum());
        SkyPoint deprecessedPoint = clickedPoint()->catalogueCoord(data->updateNum()->julianDay());
        deprecessedPoint.catalogueCoord(data->updateNum()->julianDay());
        ra                        = deprecessedPoint.ra();
        dec                       = deprecessedPoint.dec();
    }

    RAString  = QString::asprintf("ra=%f", ra.Degrees());
    DecString = QString::asprintf("&dec=%f", dec.Degrees());

    //concat all the segments into the kview command line:
    QUrl url(URLprefix + RAString + DecString + URLsuffix);

    KStars *kstars = KStars::Instance();
    if (kstars)
    {
        new ImageViewer(url,
                        i18n("Sloan Digital Sky Survey image provided by the Astrophysical Research Consortium [free "
                             "for non-commercial use]."),
                        this);
        //iv->show();
    }
}

void SkyMap::slotEyepieceView()
{
    KStars::Instance()->slotEyepieceView((clickedObject() ? clickedObject() : clickedPoint()));
}
void SkyMap::slotBeginAngularDistance()
{
    beginRulerMode(false);
}

void SkyMap::slotBeginStarHop()
{
    beginRulerMode(true);
}

void SkyMap::beginRulerMode(bool starHopRuler)
{
    rulerMode         = true;
    starHopDefineMode = starHopRuler;
    AngularRuler.clear();

    //If the cursor is near a SkyObject, reset the AngularRuler's
    //start point to the position of the SkyObject
    double maxrad = 1000.0 / Options::zoomFactor();
    SkyObject *so = data->skyComposite()->objectNearest(clickedPoint(), maxrad);
    if (so)
    {
        AngularRuler.append(so);
        AngularRuler.append(so);
        m_rulerStartPoint = so;
    }
    else
    {
        AngularRuler.append(clickedPoint());
        AngularRuler.append(clickedPoint());
        m_rulerStartPoint = clickedPoint();
    }

    AngularRuler.update(data);
}

void SkyMap::slotEndRulerMode()
{
    if (!rulerMode)
        return;
    if (!starHopDefineMode) // Angular Ruler
    {
        QString sbMessage;

        //If the cursor is near a SkyObject, reset the AngularRuler's
        //end point to the position of the SkyObject
        double maxrad = 1000.0 / Options::zoomFactor();
        SkyPoint *rulerEndPoint;
        SkyObject *so = data->skyComposite()->objectNearest(clickedPoint(), maxrad);
        if (so)
        {
            AngularRuler.setPoint(1, so);
            sbMessage     = so->translatedLongName() + "   ";
            rulerEndPoint = so;
        }
        else
        {
            AngularRuler.setPoint(1, clickedPoint());
            rulerEndPoint = clickedPoint();
        }

        rulerMode = false;
        AngularRuler.update(data);
        dms angularDistance = AngularRuler.angularSize();

        sbMessage += i18n("Angular distance: %1", angularDistance.toDMSString());

        const StarObject *p1 = dynamic_cast<const StarObject *>(m_rulerStartPoint);
        const StarObject *p2 = dynamic_cast<const StarObject *>(rulerEndPoint);

        qCDebug(KSTARS) << "Starobjects? " << p1 << p2;
        if (p1 && p2)
            qCDebug(KSTARS) << "Distances: " << p1->distance() << "pc; " << p2->distance() << "pc";
        if (p1 && p2 && std::isfinite(p1->distance()) && std::isfinite(p2->distance()) && p1->distance() > 0 &&
                p2->distance() > 0)
        {
            double dist = sqrt(p1->distance() * p1->distance() + p2->distance() * p2->distance() -
                               2 * p1->distance() * p2->distance() * cos(angularDistance.radians()));
            qCDebug(KSTARS) << "Could calculate physical distance: " << dist << " pc";
            sbMessage += i18n("; Physical distance: %1 pc", QString::number(dist));
        }

        AngularRuler.clear();

        // Create unobsructive message box with suicidal tendencies
        // to display result.
        InfoBoxWidget *box = new InfoBoxWidget(true, mapFromGlobal(QCursor::pos()), 0, QStringList(sbMessage), this);
        connect(box, SIGNAL(clicked()), box, SLOT(deleteLater()));
        QTimer::singleShot(5000, box, SLOT(deleteLater()));
        box->adjust();
        box->show();
    }
    else // Star Hop
    {
        StarHopperDialog *shd    = new StarHopperDialog(this);
        const SkyPoint &startHop = *AngularRuler.point(0);
        const SkyPoint &stopHop  = *clickedPoint();
        double fov; // Field of view in arcminutes
        bool ok;    // true if user did not cancel the operation
        if (data->getAvailableFOVs().size() == 1)
        {
            // Exactly 1 FOV symbol visible, so use that. Also assume a circular FOV of size min{sizeX, sizeY}
            FOV *f = data->getAvailableFOVs().first();
            fov    = ((f->sizeX() >= f->sizeY() && f->sizeY() != 0) ? f->sizeY() : f->sizeX());
            ok     = true;
        }
        else if (!data->getAvailableFOVs().isEmpty())
        {
            // Ask the user to choose from a list of available FOVs.
            FOV const *f;
            QMap<QString, double> nameToFovMap;
            foreach (f, data->getAvailableFOVs())
            {
                nameToFovMap.insert(f->name(),
                                    ((f->sizeX() >= f->sizeY() && f->sizeY() != 0) ? f->sizeY() : f->sizeX()));
            }
            fov = nameToFovMap[QInputDialog::getItem(this, i18n("Star Hopper: Choose a field-of-view"),
                                                           i18n("FOV to use for star hopping:"), nameToFovMap.uniqueKeys(), 0,
                                                           false, &ok)];
        }
        else
        {
            // Ask the user to enter a field of view
            fov =
                QInputDialog::getDouble(this, i18n("Star Hopper: Enter field-of-view to use"),
                                        i18n("FOV to use for star hopping (in arcminutes):"), 60.0, 1.0, 600.0, 1, &ok);
        }

        Q_ASSERT(fov > 0.0);

        if (ok)
        {
            qCDebug(KSTARS) << "fov = " << fov;

            shd->starHop(startHop, stopHop, fov / 60.0, 9.0); //FIXME: Hardcoded maglimit value
            shd->show();
        }

        rulerMode = false;
    }
}

void SkyMap::slotCancelRulerMode(void)
{
    rulerMode = false;
    AngularRuler.clear();
}

void SkyMap::slotAddFlag()
{
    KStars *ks = KStars::Instance();

    // popup FlagManager window and update coordinates
    ks->slotFlagManager();
    ks->flagManager()->clearFields();

    //ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
    //if we clicked on empty sky, we need to precess to J2000.

    dms J2000RA, J2000DE;

    if (clickedObject())
    {
        J2000RA = clickedObject()->ra0();
        J2000DE = clickedObject()->dec0();
    }
    else
    {
        //SkyPoint deprecessedPoint = clickedPoint()->deprecess(data->updateNum());
        SkyPoint deprecessedPoint = clickedPoint()->catalogueCoord(data->updateNum()->julianDay());
        deprecessedPoint.catalogueCoord(data->updateNum()->julianDay());
        J2000RA                   = deprecessedPoint.ra();
        J2000DE                   = deprecessedPoint.dec();
    }

    ks->flagManager()->setRaDec(J2000RA, J2000DE);
}

void SkyMap::slotEditFlag(int flagIdx)
{
    KStars *ks = KStars::Instance();

    // popup FlagManager window and switch to selected flag
    ks->slotFlagManager();
    ks->flagManager()->showFlag(flagIdx);
}

void SkyMap::slotDeleteFlag(int flagIdx)
{
    KStars *ks = KStars::Instance();

    ks->data()->skyComposite()->flags()->remove(flagIdx);
    ks->data()->skyComposite()->flags()->saveToFile();

    // if there is FlagManager created, update its flag model
    if (ks->flagManager())
    {
        ks->flagManager()->deleteFlagItem(flagIdx);
    }
}

void SkyMap::slotImage()
{
    const auto *action = qobject_cast<QAction *>(sender());
    const auto url     = action->data().toUrl();
    const QString message{ action->text().remove('&') };

    if (!url.isEmpty())
        new ImageViewer(url, clickedObject()->messageFromTitle(message), this);
}

void SkyMap::slotInfo()
{
    const auto *action = qobject_cast<QAction *>(sender());
    const auto url     = action->data().toUrl();

    if (!url.isEmpty())
        QDesktopServices::openUrl(url);
}

bool SkyMap::isObjectLabeled(SkyObject *object)
{
    return data->skyComposite()->labelObjects().contains(object);
}

SkyPoint SkyMap::getCenterPoint()
{
    SkyPoint retVal;
    // FIXME: subtraction of these 0.00001 is a simple workaround, because wrong
    // SkyPoint is returned when _exact_ center of SkyMap is passed to the projector.
    retVal = projector()->fromScreen(QPointF((qreal)width() / 2 - 0.00001, (qreal)height() / 2 - 0.00001), data->lst(),
                                     data->geo()->lat());
    return retVal;
}

void SkyMap::slotRemoveObjectLabel()
{
    data->skyComposite()->removeNameLabel(clickedObject());
    forceUpdate();
}

void SkyMap::slotRemoveCustomObject()
{
    auto *object = dynamic_cast<CatalogObject *>(clickedObject());
    if (!object)
        return;

    const auto &cat = object->getCatalog();
    if (!cat.mut)
        return;

    CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };
    manager.remove_object(cat.id, object->getObjectId());

    emit removeSkyObject(object);
    data->skyComposite()->removeFromNames(object);
    data->skyComposite()->removeFromLists(object);
    data->skyComposite()->reloadDeepSky();
    KStars::Instance()->updateTime();
}

void SkyMap::slotAddObjectLabel()
{
    data->skyComposite()->addNameLabel(clickedObject());
    forceUpdate();
}

void SkyMap::slotRemovePlanetTrail()
{
    TrailObject *tobj = dynamic_cast<TrailObject *>(clickedObject());
    if (tobj)
    {
        tobj->clearTrail();
        forceUpdate();
    }
}

void SkyMap::slotAddPlanetTrail()
{
    TrailObject *tobj = dynamic_cast<TrailObject *>(clickedObject());
    if (tobj)
    {
        tobj->addToTrail();
        forceUpdate();
    }
}

void SkyMap::slotDetail()
{
    // check if object is selected
    if (!clickedObject())
    {
        KMessageBox::sorry(this, i18n("No object selected."), i18n("Object Details"));
        return;
    }
    DetailDialog *detail = new DetailDialog(clickedObject(), data->ut(), data->geo(), KStars::Instance());
    detail->setAttribute(Qt::WA_DeleteOnClose);
    detail->show();
}

void SkyMap::slotObjectSelected()
{
    if (m_objPointingMode && KStars::Instance()->printingWizard())
    {
        KStars::Instance()->printingWizard()->pointingDone(clickedObject());
        m_objPointingMode = false;
    }
}

void SkyMap::slotCancelLegendPreviewMode()
{
    m_previewLegend = false;
    forceUpdate(true);
    KStars::Instance()->showImgExportDialog();
}

void SkyMap::slotFinishFovCaptureMode()
{
    if (m_fovCaptureMode && KStars::Instance()->printingWizard())
    {
        KStars::Instance()->printingWizard()->fovCaptureDone();
        m_fovCaptureMode = false;
    }
}

void SkyMap::slotCaptureFov()
{
    if (KStars::Instance()->printingWizard())
    {
        KStars::Instance()->printingWizard()->captureFov();
    }
}

void SkyMap::slotClockSlewing()
{
    //If the current timescale exceeds slewTimeScale, set clockSlewing=true, and stop the clock.
    if ((fabs(data->clock()->scale()) > Options::slewTimeScale()) ^ clockSlewing)
    {
        data->clock()->setManualMode(!clockSlewing);
        clockSlewing = !clockSlewing;
        // don't change automatically the DST status
        KStars *kstars = KStars::Instance();
        if (kstars)
            kstars->updateTime(false);
    }
}

void SkyMap::setFocus(SkyPoint *p)
{
    setFocus(p->ra(), p->dec());
}

void SkyMap::setFocus(const dms &ra, const dms &dec)
{
    Options::setFocusRA(ra.Hours());
    Options::setFocusDec(dec.Degrees());

    focus()->set(ra, dec);
    focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
}

void SkyMap::setFocusAltAz(const dms &alt, const dms &az)
{
    Options::setFocusRA(focus()->ra().Hours());
    Options::setFocusDec(focus()->dec().Degrees());
    focus()->setAlt(alt);
    focus()->setAz(az);
    focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());

    slewing = false;
    forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::setDestination(const SkyPoint &p)
{
    setDestination(p.ra(), p.dec());
}

void SkyMap::setDestination(const dms &ra, const dms &dec)
{
    destination()->set(ra, dec);
    destination()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    emit destinationChanged();
}

void SkyMap::setDestinationAltAz(const dms &alt, const dms &az, bool altIsRefracted)
{
    if (altIsRefracted)
    {
        // The alt in the SkyPoint is always actual, not apparent
        destination()->setAlt(SkyPoint::unrefract(alt));
    }
    else
    {
        destination()->setAlt(alt);
    }
    destination()->setAz(az);
    destination()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
    emit destinationChanged();
}

void SkyMap::setClickedPoint(const SkyPoint *f)
{
    ClickedPoint = *f;
}

void SkyMap::updateFocus()
{
    if (slewing)
        return;

    //Tracking on an object
    if (Options::isTracking() && focusObject() != nullptr)
    {
        if (Options::useAltAz())
        {
            //Tracking any object in Alt/Az mode requires focus updates
            focusObject()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            setFocusAltAz(focusObject()->alt(), focusObject()->az());
            focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
            setDestination(*focus());
        }
        else
        {
            //Tracking in equatorial coords
            setFocus(focusObject());
            focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            setDestination(*focus());
        }

        //Tracking on empty sky
    }
    else if (Options::isTracking() && focusPoint() != nullptr)
    {
        if (Options::useAltAz())
        {
            //Tracking on empty sky in Alt/Az mode
            setFocus(focusPoint());
            focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            setDestination(*focus());
        }

        // Not tracking and not slewing, let sky drift by
        // This means that horizontal coordinates are constant.
    }
    else
    {
        focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
    }
}

void SkyMap::slewFocus()
{
    //Don't slew if the mouse button is pressed
    //Also, no animated slews if the Manual Clock is active
    //08/2002: added possibility for one-time skipping of slew with snapNextFocus
    if (!mouseButtonDown)
    {
        bool goSlew = (Options::useAnimatedSlewing() && !data->snapNextFocus()) &&
                      !(data->clock()->isManualMode() && data->clock()->isActive());
        if (goSlew)
        {
            double dX, dY;
            double maxstep = 10.0;
            if (Options::useAltAz())
            {
                dX = destination()->az().Degrees() - focus()->az().Degrees();
                dY = destination()->alt().Degrees() - focus()->alt().Degrees();
            }
            else
            {
                dX = destination()->ra().Degrees() - focus()->ra().Degrees();
                dY = destination()->dec().Degrees() - focus()->dec().Degrees();
            }

            //switch directions to go the short way around the celestial sphere, if necessary.
            dX = KSUtils::reduceAngle(dX, -180.0, 180.0);

            double r0 = sqrt(dX * dX + dY * dY);
            if (r0 < 20.0) //smaller slews have smaller maxstep
            {
                maxstep *= (10.0 + 0.5 * r0) / 20.0;
            }
            double step = 0.5;
            double r    = r0;
            while (r > step)
            {
                //DEBUG
                //qDebug() << step << ": " << r << ": " << r0;
                double fX = dX / r;
                double fY = dY / r;

                if (Options::useAltAz())
                {
                    focus()->setAlt(focus()->alt().Degrees() + fY * step);
                    focus()->setAz(dms(focus()->az().Degrees() + fX * step).reduce());
                    focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
                }
                else
                {
                    fX = fX / 15.; //convert RA degrees to hours
                    SkyPoint newFocus(focus()->ra().Hours() + fX * step, focus()->dec().Degrees() + fY * step);
                    setFocus(&newFocus);
                    focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
                }

                slewing = true;

                forceUpdate();
                qApp->processEvents(); //keep up with other stuff

                if (Options::useAltAz())
                {
                    dX = destination()->az().Degrees() - focus()->az().Degrees();
                    dY = destination()->alt().Degrees() - focus()->alt().Degrees();
                }
                else
                {
                    dX = destination()->ra().Degrees() - focus()->ra().Degrees();
                    dY = destination()->dec().Degrees() - focus()->dec().Degrees();
                }

                //switch directions to go the short way around the celestial sphere, if necessary.
                dX = KSUtils::reduceAngle(dX, -180.0, 180.0);
                r  = sqrt(dX * dX + dY * dY);

                //Modify step according to a cosine-shaped profile
                //centered on the midpoint of the slew
                //NOTE: don't allow the full range from -PI/2 to PI/2
                //because the slew will never reach the destination as
                //the speed approaches zero at the end!
                double t = dms::PI * (r - 0.5 * r0) / (1.05 * r0);
                step     = cos(t) * maxstep;
            }
        }

        //Either useAnimatedSlewing==false, or we have slewed, and are within one step of destination
        //set focus=destination.
        if (Options::useAltAz())
        {
            setFocusAltAz(destination()->alt(), destination()->az());
            focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
        }
        else
        {
            setFocus(destination());
            focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
        }

        slewing = false;

        //Turn off snapNextFocus, we only want it to happen once
        if (data->snapNextFocus())
        {
            data->setSnapNextFocus(false);
        }

        //Start the HoverTimer. if the user leaves the mouse in place after a slew,
        //we want to attach a label to the nearest object.
        if (Options::useHoverLabel())
            m_HoverTimer.start(HOVER_INTERVAL);

        forceUpdate();
    }
}

void SkyMap::slotZoomIn()
{
    setZoomFactor(Options::zoomFactor() * DZOOM);
}

void SkyMap::slotZoomOut()
{
    setZoomFactor(Options::zoomFactor() / DZOOM);
}

void SkyMap::slotZoomDefault()
{
    setZoomFactor(DEFAULTZOOM);
}

void SkyMap::setZoomFactor(double factor)
{
    Options::setZoomFactor(KSUtils::clamp(factor, MINZOOM, MAXZOOM));
    forceUpdate();
    emit zoomChanged();
}

// force a new calculation of the skymap (used instead of update(), which may skip the redraw)
// if now=true, SkyMap::paintEvent() is run immediately, rather than being added to the event queue
// also, determine new coordinates of mouse cursor.
void SkyMap::forceUpdate(bool now)
{
    QPoint mp(mapFromGlobal(QCursor::pos()));
    if (!projector()->unusablePoint(mp))
    {
        //determine RA, Dec of mouse pointer
        m_MousePoint = projector()->fromScreen(mp, data->lst(), data->geo()->lat());
    }

    computeSkymap = true;

    // Ensure that stars are recomputed
    data->incUpdateID();

    if (now)
        m_SkyMapDraw->repaint();
    else
        m_SkyMapDraw->update();
}

float SkyMap::fov()
{
    float diagonalPixels = sqrt(static_cast<double>(width() * width() + height() * height()));
    return diagonalPixels / (2 * Options::zoomFactor() * dms::DegToRad);
}

void SkyMap::setupProjector()
{
    //Update View Parameters for projection
    ViewParams p;
    p.focus         = focus();
    p.height        = height();
    p.width         = width();
    p.useAltAz      = Options::useAltAz();
    p.useRefraction = Options::useRefraction();
    p.zoomFactor    = Options::zoomFactor();
    p.fillGround    = Options::showGround();
    //Check if we need a new projector
    if (m_proj && Options::projection() == m_proj->type())
        m_proj->setViewParams(p);
    else
    {
        delete m_proj;
        switch (Options::projection())
        {
            case Gnomonic:
                m_proj = new GnomonicProjector(p);
                break;
            case Stereographic:
                m_proj = new StereographicProjector(p);
                break;
            case Orthographic:
                m_proj = new OrthographicProjector(p);
                break;
            case AzimuthalEquidistant:
                m_proj = new AzimuthalEquidistantProjector(p);
                break;
            case Equirectangular:
                m_proj = new EquirectangularProjector(p);
                break;
            case Lambert:
            default:
                //TODO: implement other projection classes
                m_proj = new LambertProjector(p);
                break;
        }
    }
}

void SkyMap::setZoomMouseCursor()
{
    mouseMoveCursor = false; // no mousemove cursor
    QBitmap cursor  = zoomCursorBitmap(2);
    QBitmap mask    = zoomCursorBitmap(4);
    setCursor(QCursor(cursor, mask));
}

void SkyMap::setMouseCursorShape(Cursor type)
{
    // no mousemove cursor
    mouseMoveCursor = false;

    switch (type)
    {
        case Cross:
        {
            QBitmap cursor  = defaultCursorBitmap(2);
            QBitmap mask    = defaultCursorBitmap(3);
            setCursor(QCursor(cursor, mask));
        }
        break;

        case Circle:
        {
            QBitmap cursor  = circleCursorBitmap(2);
            QBitmap mask    = circleCursorBitmap(3);
            setCursor(QCursor(cursor, mask));
        }
        break;

        case NoCursor:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void SkyMap::setMouseMoveCursor()
{
    if (mouseButtonDown)
    {
        setCursor(Qt::SizeAllCursor); // cursor shape defined in qt
        mouseMoveCursor = true;
    }
}

void SkyMap::updateAngleRuler()
{
    if (rulerMode && (!pmenu || !pmenu->isVisible()))
        AngularRuler.setPoint(1, &m_MousePoint);
    AngularRuler.update(data);
}

bool SkyMap::isSlewing() const
{
    return (slewing || (clockSlewing && data->clock()->isActive()));
}

void SkyMap::slotStartXplanetViewer()
{
    if(clickedObject())
        new XPlanetImageViewer(clickedObject()->name(), this);
    else
        new XPlanetImageViewer(i18n("Saturn"), this);
}


