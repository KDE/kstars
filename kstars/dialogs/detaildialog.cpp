/*
    SPDX-FileCopyrightText: 2002 Jason Harris and Jasem Mutlaq <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "detaildialog.h"

#include "config-kstars.h"

#include "addlinkdialog.h"
#include "kspaths.h"
#include "kstars.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "observinglist.h"
#include "skymap.h"
#include "skyobjectuserdata.h"
#include "thumbnailpicker.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/skymapcomposite.h"
#include "catalogobject.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/starobject.h"
#include "skyobjects/supernova.h"
#include "catalogsdb.h"
#include "Options.h"

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#endif

#include <QDesktopServices>
#include <QDir>

DetailDialog::DetailDialog(SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo,
                           QWidget *parent)
    : KPageDialog(parent), selectedObject(o), Data(nullptr), DataComet(nullptr),
      Pos(nullptr), Links(nullptr), Adv(nullptr),
      Log(nullptr), m_user_data{ KStarsData::Instance()->getUserData(o->name()) }
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    setFaceType(Tabbed);
    setBackgroundRole(QPalette::Base);

    titlePalette = palette();
    titlePalette.setColor(backgroundRole(), palette().color(QPalette::Active, QPalette::Highlight));
    titlePalette.setColor(foregroundRole(), palette().color(QPalette::Active, QPalette::HighlightedText));

    //Create thumbnail image
    Thumbnail.reset(new QPixmap(200, 200));

    setWindowTitle(i18nc("@title:window", "Object Details"));

    // JM 2016-11-22: Do we really need a close button?
    //setStandardButtons(QDialogButtonBox::Close);
    setStandardButtons(QDialogButtonBox::NoButton);

    createGeneralTab();
    createPositionTab(ut, geo);
    createLinksTab();
    createAdvancedTab();
    createLogTab();
}

void DetailDialog::createGeneralTab()
{
    Data = new DataWidget(this);
    addPage(Data, i18n("General"));

    Data->Names->setPalette(titlePalette);

    //Connections
    connect(Data->ObsListButton, SIGNAL(clicked()), this, SLOT(addToObservingList()));
    connect(Data->CenterButton, SIGNAL(clicked()), this, SLOT(centerMap()));
#ifdef HAVE_INDI
    connect(Data->ScopeButton, SIGNAL(clicked()), this, SLOT(centerTelescope()));
#else
    Data->ScopeButton->setEnabled(false);
#endif
    connect(Data->Image, SIGNAL(clicked()), this, SLOT(updateThumbnail()));

    // Stuff that should be visible only for specific types of objects
    Data->IllumLabel->setVisible(false); // Only shown for the moon
    Data->Illumination->setVisible(false);

    Data->BVIndex->setVisible(false); // Only shown for stars
    Data->BVLabel->setVisible(false);

    Data->CatalogLabel->setVisible(false);

    //Show object thumbnail image
    showThumbnail();

    //Fill in the data fields
    //Contents depend on type of object
    QString objecttyp;

    switch (selectedObject->type())
    {
        case SkyObject::STAR:
        {
            StarObject *s = (StarObject *)selectedObject;

            if (s->getHDIndex())
            {
                Data->Names->setText(
                    QString("%1, HD %2").arg(s->longname()).arg(s->getHDIndex()));
            }
            else
            {
                Data->Names->setText(s->longname());
            }

            objecttyp = i18n("%1 star", s->sptype());
            Data->Magnitude->setText(
                i18nc("number in magnitudes", "%1 mag",
                      QLocale().toString(s->mag(), 'f', 2))); //show to hundredth place

            Data->BVLabel->setVisible(true);
            Data->BVIndex->setVisible(true);
            if (s->getBVIndex() < 30.)
            {
                Data->BVIndex->setText(QString::number(s->getBVIndex(), 'f', 2));
            }

            //The thumbnail image is empty, and isn't clickable for stars
            //Also, don't show the border around the Image QFrame.
            Data->Image->setFrameStyle(QFrame::NoFrame);
            disconnect(Data->Image, SIGNAL(clicked()), this, SLOT(updateThumbnail()));

            //distance
            if (s->distance() > 2000. || s->distance() < 0.) // parallax < 0.5 mas
            {
                Data->Distance->setText(
                    QString(i18nc("larger than 2000 parsecs", "> 2000 pc")));
            }
            else if (s->distance() > 50.) //show to nearest integer
            {
                Data->Distance->setText(i18nc("number in parsecs", "%1 pc",
                                              QLocale().toString(s->distance(), 'f', 0)));
            }
            else if (s->distance() > 10.0) //show to tenths place
            {
                Data->Distance->setText(i18nc("number in parsecs", "%1 pc",
                                              QLocale().toString(s->distance(), 'f', 1)));
            }
            else //show to hundredths place
            {
                Data->Distance->setText(i18nc("number in parsecs", "%1 pc",
                                              QLocale().toString(s->distance(), 'f', 2)));
            }

            //Note multiplicity/variability in angular size label
            Data->AngSizeLabel->setText(QString());
            Data->AngSize->setText(QString());
            Data->AngSizeLabel->setFont(Data->AngSize->font());
            if (s->isMultiple() && s->isVariable())
            {
                Data->AngSizeLabel->setText(
                    i18nc("the star is a multiple star", "multiple") + ',');
                Data->AngSize->setText(i18nc("the star is a variable star", "variable"));
            }
            else if (s->isMultiple())
            {
                Data->AngSizeLabel->setText(
                    i18nc("the star is a multiple star", "multiple"));
            }
            else if (s->isVariable())
            {
                Data->AngSizeLabel->setText(
                    i18nc("the star is a variable star", "variable"));
            }

            // Add a label to indicate proper motion
            double pmRA = s->pmRA(), pmDec = s->pmDec();
            if (std::isfinite(pmRA) && std::isfinite(pmDec) && (pmRA != 0.0 || pmDec != 0.0))
            {
                // we have data : abuse the illumination label to show it!
                Data->IllumLabel->setText(i18nc("Proper motion of a star", "Proper Motion:"));
                Data->Illumination->setText(
                    i18nc(
                        "The first arg is proper motion in right ascension and the second in the declination. The unit stands for milliarcsecond per year",
                        "%1 %2 mas/yr",
                        QLocale().toString(pmRA, 'f', (pmRA >= 100.0 ? 1 :  2)),
                        QLocale().toString(pmDec, 'f', (pmDec >= 100.0 ? 1 : 2))
                    ));
                Data->IllumLabel->setVisible(true);
                Data->Illumination->setVisible(true);
            }

            break; //end of stars case
        }
        case SkyObject::ASTEROID: //[fall through to planets]
        case SkyObject::COMET:    //[fall through to planets]
        case SkyObject::MOON:     //[fall through to planets]
        case SkyObject::PLANET:
        {
            KSPlanetBase *ps = dynamic_cast<KSPlanetBase *>(selectedObject);

            if (!ps)
                break;

            Data->Names->setText(ps->longname());

            //Type is "G5 star" for Sun
            if (ps->name() == i18n("Sun"))
            {
                objecttyp = i18n("G5 star");
            }
            else if (ps->name() == i18n("Moon"))
            {
                objecttyp = ps->translatedName();
            }
            else if (ps->name() == i18nc("Asteroid name (optional)", "Pluto") ||
                     ps->name() == i18nc("Asteroid name (optional)", "Ceres") ||
                     ps->name() == i18nc("Asteroid name (optional)", "Eris"))
            {
                objecttyp = i18n("Dwarf planet");
            }
            else
            {
                objecttyp = ps->typeName();
            }

            //The moon displays illumination fraction and updateMag is called to calculate moon's current magnitude
            if (selectedObject->name() == i18n("Moon"))
            {
                Data->IllumLabel->setVisible(true);
                Data->Illumination->setVisible(true);
                Data->Illumination->setText(QString("%1 %").arg(QLocale().toString(
                                                ((KSMoon *)selectedObject)->illum() * 100., 'f', 0)));
                ((KSMoon *)selectedObject)->updateMag();
            }

            // JM: Shouldn't we use the calculated magnitude? Disabling the following
            /*
            if(selectedObject->type() == SkyObject::COMET){
                Data->Magnitude->setText(i18nc("number in magnitudes", "%1 mag",
                                         QLocale().toString( ((KSComet *)selectedObject)->getTotalMagnitudeParameter(), 'f', 2)));  //show to hundredth place

            }
            else{*/
            Data->Magnitude->setText(
                i18nc("number in magnitudes", "%1 mag",
                      QLocale().toString(ps->mag(), 'f', 2))); //show to hundredth place
            //}

            //Distance from Earth.  The moon requires a unit conversion
            if (ps->name() == i18n("Moon"))
            {
                Data->Distance->setText(
                    i18nc("distance in kilometers", "%1 km",
                          QLocale().toString(ps->rearth() * AU_KM, 'f', 2)));
            }
            else
            {
                Data->Distance->setText(i18nc("distance in Astronomical Units", "%1 AU",
                                              QLocale().toString(ps->rearth(), 'f', 3)));
            }

            //Angular size; moon and sun in arcmin, others in arcsec
            if (ps->angSize())
            {
                if (ps->name() == i18n("Sun") || ps->name() == i18n("Moon"))
                {
                    Data->AngSize->setText(i18nc(
                                               "angular size in arcminutes", "%1 arcmin",
                                               QLocale().toString(
                                                   ps->angSize(), 'f',
                                                   1))); // Needn't be a plural form because sun / moon will never contract to 1 arcminute
                }
                else
                {
                    Data->AngSize->setText(
                        i18nc("angular size in arcseconds", "%1 arcsec",
                              QLocale().toString(ps->angSize() * 60.0, 'f', 1)));
                }
            }
            else
            {
                Data->AngSize->setText("--");
            }

            break; //end of planets/comets/asteroids case
        }
        case SkyObject::SUPERNOVA:
        {
            Supernova *sup = dynamic_cast<Supernova *>(selectedObject);

            objecttyp = i18n("Supernova");
            Data->Names->setText(sup->name());
            if (sup->mag() < 99)
                Data->Magnitude->setText(i18nc("number in magnitudes", "%1 mag",
                                               QLocale().toString(sup->mag(), 'f', 2)));
            else
                Data->Magnitude->setText("--");

            Data->DistanceLabel->setVisible(false);
            Data->Distance->setVisible(false);

            Data->AngSizeLabel->setVisible(false);
            Data->AngSize->setVisible(false);

            QLabel *discoveryDateLabel = new QLabel(i18n("Discovery Date:"), this);
            QLabel *discoveryDate      = new QLabel(sup->getDate(), this);
            Data->dataGridLayout->addWidget(discoveryDateLabel, 1, 0);
            Data->dataGridLayout->addWidget(discoveryDate, 1, 1);

            QLabel *typeLabel = new QLabel(i18n("Type:"), this);
            QLabel *type      = new QLabel(sup->getType(), this);
            Data->dataGridLayout->addWidget(typeLabel, 2, 0);
            Data->dataGridLayout->addWidget(type, 2, 1);

            QLabel *hostGalaxyLabel = new QLabel(i18n("Host Galaxy:"), this);
            QLabel *hostGalaxy      = new QLabel(
                sup->getHostGalaxy().isEmpty() ? "--" : sup->getHostGalaxy(), this);
            Data->dataGridLayout->addWidget(hostGalaxyLabel, 3, 0);
            Data->dataGridLayout->addWidget(hostGalaxy, 3, 1);

            QLabel *redShiftLabel = new QLabel(i18n("Red Shift:"), this);
            QLabel *redShift      = new QLabel(
                (sup->getRedShift() < 99) ? QString::number(sup->getRedShift(), 'f', 2) :
                QString("--"),
                this);
            Data->dataGridLayout->addWidget(redShiftLabel, 4, 0);
            Data->dataGridLayout->addWidget(redShift, 4, 1);

            break;
        }
        default: //deep-sky objects
        {
            CatalogObject *dso = dynamic_cast<CatalogObject *>(selectedObject);

            if (!dso)
                break;

            //Show all names recorded for the object
            QStringList nameList;
            if (!dso->longname().isEmpty() && dso->longname() != dso->name())
            {
                nameList.append(dso->translatedLongName());
            }

            nameList.append(dso->translatedName());

            if (!dso->translatedName2().isEmpty())
            {
                nameList.append(dso->translatedName2());
            }

            Data->Names->setText(nameList.join(","));

            objecttyp = dso->typeName();

            if (dso->type() == SkyObject::RADIO_SOURCE)
            {
                Data->MagLabel->setText(
                    i18nc("integrated flux at a frequency", "Flux(%1):", 1));
                Data->Magnitude->setText(i18nc("integrated flux value", "%1 %2",
                                               QLocale().toString(dso->flux(), 'f', 1),
                                               "obs")); //show to tenths place
            }
            else if (std::isnan(dso->mag()))
            {
                Data->Magnitude->setText("--");
            }
            else
            {
                Data->Magnitude->setText(
                    i18nc("number in magnitudes", "%1 mag",
                          QLocale().toString(dso->mag(), 'f', 1))); //show to tenths place
            }

            //No distances at this point...
            Data->Distance->setText("--");

            Data->CatalogLabel->setVisible(true);
            Data->Catalog->setText(dso->getCatalog().name);

            //Only show decimal place for small angular sizes
            if (dso->a() > 10.0)
            {
                Data->AngSize->setText(i18nc("angular size in arcminutes", "%1 arcmin",
                                             QLocale().toString(dso->a(), 'f', 0)));
            }
            else if (dso->a())
            {
                Data->AngSize->setText(i18nc("angular size in arcminutes", "%1 arcmin",
                                             QLocale().toString(dso->a(), 'f', 1)));
            }
            else
            {
                Data->AngSize->setText("--");
            }

            break;
        }
    }

    // Add specifics data
    switch (selectedObject->type())
    {
        case SkyObject::ASTEROID:
        {
            KSAsteroid *ast = dynamic_cast<KSAsteroid *>(selectedObject);
            // Show same specifics data as comets
            DataComet = new DataCometWidget(this);
            Data->IncludeData->layout()->addWidget(DataComet);

            // Perihelion
            DataComet->Perihelion->setText(i18nc("Distance in astronomical units",
                                                 "%1 AU",
                                                 QString::number(ast->getPerihelion())));
            // Earth MOID
            if (ast->getEarthMOID() == 0)
                DataComet->EarthMOID->setText("--");
            else
                DataComet->EarthMOID->setText(
                    i18nc("Distance in astronomical units", "%1 AU",
                          QString::number(ast->getEarthMOID())));
            // Orbit ID
            DataComet->OrbitID->setText(ast->getOrbitID());
            // Orbit Class
            DataComet->OrbitClass->setText(ast->getOrbitClass());
            // NEO
            if (ast->isNEO())
                DataComet->NEO->setText(i18n("Yes"));
            else
                DataComet->NEO->setText(i18n("No"));
            // Albedo
            if (ast->getAlbedo() == 0.0)
                DataComet->Albedo->setText("--");
            else
                DataComet->Albedo->setText(QString::number(ast->getAlbedo()));
            // Diameter
            if (ast->getDiameter() == 0.0)
                DataComet->Diameter->setText("--");
            else
                DataComet->Diameter->setText(i18nc("Diameter in kilometers", "%1 km",
                                                   QString::number(ast->getDiameter())));
            // Dimensions
            if (ast->getDimensions().isEmpty())
                DataComet->Dimensions->setText("--");
            else
                DataComet->Dimensions->setText(
                    i18nc("Dimension in kilometers", "%1 km", ast->getDimensions()));
            // Rotation period
            if (ast->getRotationPeriod() == 0.0)
                DataComet->Rotation->setText("--");
            else
                DataComet->Rotation->setText(
                    i18nc("Rotation period in hours", "%1 h",
                          QString::number(ast->getRotationPeriod())));
            // Period
            if (ast->getPeriod() == 0.0)
                DataComet->Period->setText("--");
            else
                DataComet->Period->setText(i18nc("Orbit period in years", "%1 y",
                                                 QString::number(ast->getPeriod())));
            break;
        }
        case SkyObject::COMET:
        {
            KSComet *com = dynamic_cast<KSComet *>(selectedObject);
            DataComet    = new DataCometWidget(this);
            Data->IncludeData->layout()->addWidget(DataComet);

            // Perihelion
            DataComet->Perihelion->setText(i18nc("Distance in astronomical units",
                                                 "%1 AU",
                                                 QString::number(com->getPerihelion())));
            // Earth MOID
            if (com->getEarthMOID() == 0)
                DataComet->EarthMOID->setText("--");
            else
                DataComet->EarthMOID->setText(
                    i18nc("Distance in astronomical units", "%1 AU",
                          QString::number(com->getEarthMOID())));
            // Orbit ID
            DataComet->OrbitID->setText(com->getOrbitID());
            // Orbit Class
            DataComet->OrbitClass->setText(com->getOrbitClass());
            // NEO
            if (com->isNEO())
                DataComet->NEO->setText(i18n("Yes"));
            else
                DataComet->NEO->setText(i18n("No"));
            // Albedo
            if (com->getAlbedo() == 0.0)
                DataComet->Albedo->setText("--");
            else
                DataComet->Albedo->setText(QString::number(com->getAlbedo()));
            // Diameter
            if (com->getDiameter() == 0.0)
                DataComet->Diameter->setText("--");
            else
                DataComet->Diameter->setText(i18nc("Diameter in kilometers", "%1 km",
                                                   QString::number(com->getDiameter())));
            // Dimensions
            if (com->getDimensions().isEmpty())
                DataComet->Dimensions->setText("--");
            else
                DataComet->Dimensions->setText(
                    i18nc("Dimension in kilometers", "%1 km", com->getDimensions()));
            // Rotation period
            if (com->getRotationPeriod() == 0.0)
                DataComet->Rotation->setText("--");
            else
                DataComet->Rotation->setText(
                    i18nc("Rotation period in hours", "%1 h",
                          QString::number(com->getRotationPeriod())));
            // Period
            if (com->getPeriod() == 0.0)
                DataComet->Period->setText("--");
            else
                DataComet->Period->setText(i18nc("Orbit period in years", "%1 y",
                                                 QString::number(com->getPeriod())));
            break;
        }
    }

    //Common to all types:
    QString cname = KStarsData::Instance()
                    ->skyComposite()
                    ->constellationBoundary()
                    ->constellationName(selectedObject);
    if (selectedObject->type() != SkyObject::CONSTELLATION)
    {
        cname = i18nc(
                    "%1 type of sky object (planet, asteroid etc), %2 name of a constellation",
                    "%1 in %2", objecttyp, cname);
    }
    Data->ObjectTypeInConstellation->setText(cname);
}

void DetailDialog::createPositionTab(const KStarsDateTime &ut, GeoLocation *geo)
{
    Pos = new PositionWidget(this);
    addPage(Pos, i18n("Position"));

    Pos->CoordTitle->setPalette(titlePalette);
    Pos->RSTTitle->setPalette(titlePalette);
    KStarsData *data = KStarsData::Instance();

    //Coordinates Section:
    //Don't use KLocale::formatNumber() for the epoch string,
    //because we don't want a thousands-place separator!
    selectedObject->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
    QString sEpoch = QString::number(KStarsDateTime::jdToEpoch(selectedObject->getLastPrecessJD()), 'f', 1);
    //Replace the decimal point with localized decimal symbol
    sEpoch.replace('.', QLocale().decimalPoint()); // Is this necessary? -- asimha Oct 2016

    /*qDebug() << (selectedObject->deprecess(data->updateNum())).ra0().toHMSString()
             << (selectedObject->deprecess(data->updateNum())).dec0().toDMSString();*/
    //qDebug() << selectedObject->ra().toHMSString() << selectedObject->dec().toDMSString();
    Pos->RALabel->setText(i18n("RA (%1):", sEpoch));
    Pos->DecLabel->setText(i18n("DE (%1):", sEpoch));
    Pos->RA->setText(selectedObject->ra().toHMSString(false, true));
    Pos->Dec->setText(selectedObject->dec().toDMSString(false, false, true));

    selectedObject->EquatorialToHorizontal(data->lst(), data->geo()->lat());

    Pos->Az->setText(selectedObject->az().toDMSString());
    dms a;

    if (Options::useAltAz())
        a = selectedObject->alt();
    else
        a = selectedObject->altRefracted();
    Pos->Alt->setText(a.toDMSString());

    // Display the RA0 and Dec0 for objects that are outside the solar system
    // 2017-09-10 JM: Exception added for asteroids and comets since we have J2000 for them.
    // Maybe others?
    Pos->RA0->setText(selectedObject->ra0().toHMSString(false, true));
    Pos->Dec0->setText(selectedObject->dec0().toDMSString(false, false, true));
#if 0
    if (!selectedObject->isSolarSystem() || selectedObject->type() == SkyObject::COMET || selectedObject->type() == SkyObject::ASTEROID)
    {
        Pos->RA0->setText(selectedObject->ra0().toHMSString());
        Pos->Dec0->setText(selectedObject->dec0().toDMSString());
    }
    else
    {
        Pos->RA0->setText("--");
        Pos->Dec0->setText("--");
    }
#endif

    //Hour Angle can be negative, but dms HMS expressions cannot.
    //Here's a kludgy workaround:
    dms lst = geo->GSTtoLST(ut.gst());
    dms ha(lst.Degrees() - selectedObject->ra().Degrees());
    QChar sgn('+');
    if (ha.Hours() > 12.0)
    {
        ha.setH(24.0 - ha.Hours());
        sgn = '-';
    }
    Pos->HA->setText(QString("%1%2").arg(sgn).arg(ha.toHMSString()));

    //Airmass is approximated as the secant of the zenith distance,
    //equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
    if (selectedObject->alt().Degrees() > 0.0)
        Pos->Airmass->setText(QLocale().toString(selectedObject->airmass(), 'f', 2));
    else
        Pos->Airmass->setText("--");

    //Rise/Set/Transit Section:

    //Prepare time/position variables
    QTime rt = selectedObject->riseSetTime(ut, geo, true);   //true = use rise time
    dms raz  = selectedObject->riseSetTimeAz(ut, geo, true); //true = use rise time

    //If transit time is before rise time, use transit time for tomorrow
    QTime tt = selectedObject->transitTime(ut, geo);
    dms talt = selectedObject->transitAltitude(ut, geo);
    if (tt < rt)
    {
        tt   = selectedObject->transitTime(ut.addDays(1), geo);
        talt = selectedObject->transitAltitude(ut.addDays(1), geo);
    }

    // JM 2021.09.14: Set time is already taken care of
    QTime st = selectedObject->riseSetTime(ut, geo, false);   //false = use set time
    dms saz  = selectedObject->riseSetTimeAz(ut, geo, false); //false = use set time
    //    if (st < rt)
    //    {
    //        st  = selectedObject->riseSetTime(ut.addDays(1), geo, false);   //false = use set time
    //        saz = selectedObject->riseSetTimeAz(ut.addDays(1), geo, false); //false = use set time
    //    }

    if (rt.isValid())
    {
        Pos->TimeRise->setText(QString::asprintf("%02d:%02d", rt.hour(), rt.minute()));
        Pos->TimeSet->setText(QString::asprintf("%02d:%02d", st.hour(), st.minute()));
        Pos->AzRise->setText(raz.toDMSString());
        Pos->AzSet->setText(saz.toDMSString());
    }
    else
    {
        if (selectedObject->alt().Degrees() > 0.0)
        {
            Pos->TimeRise->setText(i18n("Circumpolar"));
            Pos->TimeSet->setText(i18n("Circumpolar"));
        }
        else
        {
            Pos->TimeRise->setText(i18n("Never rises"));
            Pos->TimeSet->setText(i18n("Never rises"));
        }

        Pos->AzRise->setText(i18nc("Not Applicable", "N/A"));
        Pos->AzSet->setText(i18nc("Not Applicable", "N/A"));
    }

    Pos->TimeTransit->setText(QString::asprintf("%02d:%02d", tt.hour(), tt.minute()));
    Pos->AltTransit->setText(talt.toDMSString());

    // Restore the position and other time-dependent parameters
    selectedObject->recomputeCoords(ut, geo);
}

void DetailDialog::createLinksTab()
{
    // don't create a link tab for an unnamed star
    if (selectedObject->name() == QString("star"))
        return;

    Links = new LinksWidget(this);
    addPage(Links, i18n("Links"));

    Links->InfoTitle->setPalette(titlePalette);
    Links->ImagesTitle->setPalette(titlePalette);

    for (const auto &link : m_user_data.websites())
        Links->InfoTitleList->addItem(i18nc("Image/info menu item (should be translated)",
                                            link.title.toLocal8Bit()));

    //Links->InfoTitleList->setCurrentRow(0);

    for (const auto &link : m_user_data.images())
        Links->ImageTitleList->addItem(i18nc(
                                           "Image/info menu item (should be translated)", link.title.toLocal8Bit()));

    // Signals/Slots
    connect(Links->ViewButton, SIGNAL(clicked()), this, SLOT(viewLink()));
    connect(Links->AddLinkButton, SIGNAL(clicked()), this, SLOT(addLink()));
    connect(Links->EditLinkButton, SIGNAL(clicked()), this, SLOT(editLinkDialog()));
    connect(Links->RemoveLinkButton, SIGNAL(clicked()), this, SLOT(removeLinkDialog()));

    // When an item is selected in info list, selected items are cleared image list.
    connect(Links->InfoTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this,
            SLOT(setCurrentLink(QListWidgetItem*)));
    connect(Links->InfoTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            Links->ImageTitleList, SLOT(clearSelection()));

    // vice versa
    connect(Links->ImageTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this,
            SLOT(setCurrentLink(QListWidgetItem*)));
    connect(Links->ImageTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            Links->InfoTitleList, SLOT(clearSelection()));

    connect(Links->InfoTitleList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(viewLink()));
    connect(Links->ImageTitleList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(viewLink()));

    connect(Links->InfoTitleList, SIGNAL(itemSelectionChanged()), this, SLOT(updateButtons()));
    connect(Links->ImageTitleList, SIGNAL(itemSelectionChanged()), this, SLOT(updateButtons()));

    updateLists();
}

void DetailDialog::addLink()
{
    if (!selectedObject)
        return;
    QPointer<AddLinkDialog> adialog = new AddLinkDialog(this, selectedObject->name());
    QString entry;
    QFile file;

    if (adialog->exec() == QDialog::Accepted)
    {
        const auto &success = KStarsData::Instance()->addToUserData(
                                  selectedObject->name(),
                                  SkyObjectUserdata::LinkData
        {
            adialog->desc(), QUrl(adialog->url()),
            (adialog->isImageLink()) ? SkyObjectUserdata::LinkData::Type::image :
            SkyObjectUserdata::LinkData::Type::website });

        if (!success.first)
        {
            KSNotification::sorry(success.second, i18n("Could not add the link."));
        }
    }

    updateLists();
    delete adialog;
}

void DetailDialog::createAdvancedTab()
{
    // Don't create an adv tab for an unnamed star or if advinterface file failed loading
    // We also don't need adv dialog for solar system objects.
    if (selectedObject->name() == QString("star") || KStarsData::Instance()->avdTree().isEmpty() ||
            selectedObject->type() == SkyObject::PLANET || selectedObject->type() == SkyObject::COMET ||
            selectedObject->type() == SkyObject::ASTEROID)
        return;

    Adv = new DatabaseWidget(this);
    addPage(Adv, i18n("Advanced"));

    connect(Adv->ADVTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(viewADVData()));

    populateADVTree();
}

void DetailDialog::createLogTab()
{
    //Don't create a log tab for an unnamed star
    if (selectedObject->name() == QString("star"))
        return;

    // Log Tab
    Log = new LogWidget(this);
    addPage(Log, i18n("Log"));

    Log->LogTitle->setPalette(titlePalette);

    const auto &log = m_user_data.userLog;

    if (log.isEmpty())
        Log->UserLog->setText(i18n("Record here observation logs and/or data on %1.",
                                   selectedObject->translatedName()));
    else
        Log->UserLog->setText(log);

    //Automatically save the log contents when the widget loses focus
    connect(Log->UserLog, SIGNAL(focusOut()), this, SLOT(saveLogData()));
}

void DetailDialog::setCurrentLink(QListWidgetItem *it)
{
    m_CurrentLink = it;
}

void DetailDialog::viewLink()
{
    QString URL;

    if (m_CurrentLink == nullptr)
        return;

    if (m_CurrentLink->listWidget() == Links->InfoTitleList)
    {
        URL = m_user_data.websites()
              .at(Links->InfoTitleList->row(m_CurrentLink))
              .url.toString();
    }
    else if (m_CurrentLink->listWidget() == Links->ImageTitleList)
    {
        URL = m_user_data.images()
              .at(Links->ImageTitleList->row(m_CurrentLink))
              .url.toString();
    }

    if (!URL.isEmpty())
        QDesktopServices::openUrl(QUrl(URL));
}

void DetailDialog::updateLists()
{
    Links->InfoTitleList->clear();
    Links->ImageTitleList->clear();

    for (const auto &element : m_user_data.websites())
        Links->InfoTitleList->addItem(element.title);

    for (const auto &element : m_user_data.images())
        Links->ImageTitleList->addItem(element.title);

    updateButtons();
}

void DetailDialog::updateButtons()
{
    bool anyLink = false;
    if (!Links->InfoTitleList->selectedItems().isEmpty() || !Links->ImageTitleList->selectedItems().isEmpty())
        anyLink = true;

    // Buttons could be disabled if lists are initially empty, we enable and disable them here
    // depending on the current status of the list.
    Links->ViewButton->setEnabled(anyLink);
    Links->EditLinkButton->setEnabled(anyLink);
    Links->RemoveLinkButton->setEnabled(anyLink);
}

void DetailDialog::editLinkDialog()
{
    SkyObjectUserdata::Type type = SkyObjectUserdata::Type::image;
    int row                      = 0;

    if (m_CurrentLink == nullptr)
        return;

    QDialog editDialog(this);
    editDialog.setWindowTitle(i18nc("@title:window", "Edit Link"));

    QVBoxLayout *mainLayout = new QVBoxLayout;

    QFrame editFrame(&editDialog);

    mainLayout->addWidget(&editFrame);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), &editDialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), &editDialog, SLOT(reject()));

    editDialog.setLayout(mainLayout);

    if (m_CurrentLink->listWidget() == Links->InfoTitleList)
    {
        row  = Links->InfoTitleList->row(m_CurrentLink);
        type = SkyObjectUserdata::Type::website;
    }
    else if (m_CurrentLink->listWidget() == Links->ImageTitleList)
    {
        row  = Links->ImageTitleList->row(m_CurrentLink);
        type = SkyObjectUserdata::Type::image;
    }
    else
        return;

    const auto &currentItem = m_user_data.links.at(type).at(row);

    QLineEdit editNameField(&editFrame);
    editNameField.setObjectName("nameedit");
    editNameField.home(false);
    editNameField.setText(currentItem.title);
    QLabel editLinkURL(i18n("URL:"), &editFrame);
    QLineEdit editLinkField(&editFrame);
    editLinkField.setObjectName("urledit");
    editLinkField.home(false);
    editLinkField.setText(currentItem.url.toString());
    QVBoxLayout vlay(&editFrame);
    vlay.setObjectName("vlay");
    QHBoxLayout editLinkLayout(&editFrame);
    editLinkLayout.setObjectName("editlinklayout");
    editLinkLayout.addWidget(&editLinkURL);
    editLinkLayout.addWidget(&editLinkField);
    vlay.addWidget(&editNameField);
    vlay.addLayout(&editLinkLayout);

    bool go(true);
    // If user presses cancel then skip the action
    if (editDialog.exec() != QDialog::Accepted)
        go = false;

    // If nothing changed, skip th action
    if (editLinkField.text() == currentItem.url.toString() &&
            editNameField.text() == currentItem.title)
        go = false;

    if (go)
    {
        const auto &success = KStarsData::Instance()->editUserData(
                                  selectedObject->name(), row,
        { editNameField.text(), QUrl{ editLinkField.text() }, type });

        if (!success.first)
            KSNotification::sorry(success.second, i18n("Could not edit the entry."));

        // Set focus to the same item again
        if (type == SkyObjectUserdata::Type::website)
            Links->InfoTitleList->setCurrentRow(row);
        else
            Links->ImageTitleList->setCurrentRow(row);
    }

    updateLists();
}

void DetailDialog::removeLinkDialog()
{
    int row = 0;
    if (m_CurrentLink == nullptr)
        return;

    SkyObjectUserdata::Type type;

    if (m_CurrentLink->listWidget() == Links->InfoTitleList)
    {
        row  = Links->InfoTitleList->row(m_CurrentLink);
        type = SkyObjectUserdata::Type::website;
    }
    else if (m_CurrentLink->listWidget() == Links->ImageTitleList)
    {
        row  = Links->ImageTitleList->row(m_CurrentLink);
        type = SkyObjectUserdata::Type::image;
    }
    else
        return;

    if (KMessageBox::warningContinueCancel(
                nullptr,
                i18n("Are you sure you want to remove the %1 link?", m_CurrentLink->text()),
                i18n("Delete Confirmation"),
                KStandardGuiItem::del()) != KMessageBox::Continue)
        return;

    const auto &success =
        KStarsData::Instance()->deleteUserData(selectedObject->name(), row, type);

    if (!success.first)
        KSNotification::sorry(success.second, i18n("Could not delete the entry."));

    // Set focus to the 1st item in the list
    if (type == SkyObjectUserdata::LinkData::Type::website)
        Links->InfoTitleList->clearSelection();
    else
        Links->ImageTitleList->clearSelection();
}

void DetailDialog::populateADVTree()
{
    QTreeWidgetItem *parent = nullptr;
    QTreeWidgetItem *temp   = nullptr;

    // We populate the tree iteratively, keeping track of parents as we go
    // This solution is more efficient than the previous recursion algorithm.
    foreach (ADVTreeData *item, KStarsData::Instance()->avdTree())
    {
        switch (item->Type)
        {
            // Top Level
            case 0:
                temp = new QTreeWidgetItem(parent, QStringList(i18nc("Advanced URLs: description or category", item->Name.toLocal8Bit().data())));
                if (parent == nullptr)
                    Adv->ADVTree->addTopLevelItem(temp);
                parent = temp;

                break;

            // End of top level
            case 1:
                if (parent != nullptr)
                    parent = parent->parent();
                break;

            // Leaf
            case 2:
                new QTreeWidgetItem(parent, QStringList(i18nc("Advanced URLs: description or category", item->Name.toLocal8Bit().data())));
                break;
        }
    }
}

void DetailDialog::viewADVData()
{
    QString link;
    QTreeWidgetItem *current = Adv->ADVTree->currentItem();

    //If the item has children or is invalid, do nothing
    if (!current || current->childCount() > 0)
        return;

    foreach (ADVTreeData *item, KStarsData::Instance()->avdTree())
    {
        if (item->Name == current->text(0))
        {
            link = item->Link;
            link = parseADVData(link);
            QDesktopServices::openUrl(QUrl(link));
            return;
        }
    }
}

QString DetailDialog::parseADVData(const QString &inlink)
{
    QString link = inlink;
    QString subLink;
    int index;

    if ((index = link.indexOf("KSOBJ")) != -1)
    {
        link.remove(index, 5);
        link = link.insert(index, selectedObject->name());
    }

    if ((index = link.indexOf("KSRA")) != -1)
    {
        link.remove(index, 4);
        subLink = QString::asprintf("%02d%02d%02d", selectedObject->ra0().hour(), selectedObject->ra0().minute(),
                                    selectedObject->ra0().second());
        subLink = subLink.insert(2, "%20");
        subLink = subLink.insert(7, "%20");

        link = link.insert(index, subLink);
    }
    if ((index = link.indexOf("KSDEC")) != -1)
    {
        link.remove(index, 5);
        if (selectedObject->dec().degree() < 0)
        {
            subLink = QString::asprintf("%03d%02d%02d", selectedObject->dec0().degree(), selectedObject->dec0().arcmin(),
                                        selectedObject->dec0().arcsec());
            subLink = subLink.insert(3, "%20");
            subLink = subLink.insert(8, "%20");
        }
        else
        {
            subLink = QString::asprintf("%02d%02d%02d", selectedObject->dec0().degree(), selectedObject->dec0().arcmin(),
                                        selectedObject->dec0().arcsec());
            subLink = subLink.insert(0, "%2B");
            subLink = subLink.insert(5, "%20");
            subLink = subLink.insert(10, "%20");
        }
        link = link.insert(index, subLink);
    }

    return link;
}

void DetailDialog::saveLogData()
{
    const auto &success = KStarsData::Instance()->updateUserLog(
                              selectedObject->name(), Log->UserLog->toPlainText());

    if (!success.first)
        KSNotification::sorry(success.second, i18n("Could not update the user log."));
}

void DetailDialog::addToObservingList()
{
    KStarsData::Instance()->observingList()->slotAddObject(selectedObject);
}

void DetailDialog::centerMap()
{
    SkyMap::Instance()->setClickedObject(selectedObject);
    SkyMap::Instance()->slotCenter();
}

void DetailDialog::centerTelescope()
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KSNotification::sorry(i18n("KStars did not find any active telescopes."));
        return;
    }

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == nullptr)
            continue;

        if (bd->isConnected() == false)
        {
            KSNotification::error(
                i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }

        // Display Sun warning on slew
        if (selectedObject && selectedObject->name() == i18n("Sun"))
        {
            if (KMessageBox::warningContinueCancel(nullptr, i18n("Danger! Viewing the Sun without adequate solar filters is dangerous and will result in permanent eye damage!"))
                    == KMessageBox::Cancel)
                return;
        }

        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS, selectedObject);

        return;
    }

    KSNotification::sorry(i18n("KStars did not find any active telescopes."));

#endif
}

void DetailDialog::showThumbnail()
{
    //No image if object is a star
    if (selectedObject->type() == SkyObject::STAR ||
            selectedObject->type() == SkyObject::CATALOG_STAR)
    {
        Thumbnail->scaled(Data->Image->width(), Data->Image->height());
        Thumbnail->fill(Data->DataFrame->palette().color(QPalette::Window));
        Data->Image->setPixmap(*Thumbnail);
        return;
    }

    //Try to load the object's image from disk
    //If no image found, load "no image" image

    const auto &base = KSPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDirIterator search(base, QStringList() << "thumb*", QDir::Dirs);

    bool found = false;
    while (search.hasNext())
    {
        const auto &path =
            QDir(search.next())
            .absoluteFilePath(
                "thumb-" + selectedObject->name().toLower().remove(' ').remove('/') +
                ".png");

        const QFile file{ path };
        if (file.exists())
        {
            Thumbnail->load(path, "PNG");
            found = true;
        }
    }

    if (!found)
        Thumbnail->load(":/images/noimage.png");

    *Thumbnail = Thumbnail->scaled(Data->Image->width(), Data->Image->height(),
                                   Qt::KeepAspectRatio, Qt::FastTransformation);

    Data->Image->setPixmap(*Thumbnail);
}

void DetailDialog::updateThumbnail()
{
    QPointer<ThumbnailPicker> tp = new ThumbnailPicker(selectedObject, *Thumbnail, this);

    if (tp->exec() == QDialog::Accepted)
    {
        QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).mkpath("thumbnails");

        QString const fname =
            QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath("thumb-" + selectedObject->name().toLower().remove(' ').remove('/') + ".png");

        Data->Image->setPixmap(*(tp->image()));

        //If a real image was set, save it.
        //If the image was unset, delete the old image on disk.
        if (tp->imageFound())
        {
            bool rc = Data->Image->pixmap()->save(fname, "PNG");
            if (rc == false)
            {
                KSNotification::error(i18n("Unable to save image to %1", fname),
                                      i18n("Save Thumbnail"));
            }
            else
                *Thumbnail = *(Data->Image->pixmap());
        }
        else
        {
            QFile f;
            f.setFileName(fname);
            f.remove();
        }
    }
    delete tp;
}

DataWidget::DataWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
    DataFrame->setBackgroundRole(QPalette::Base);
}

DataCometWidget::DataCometWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

PositionWidget::PositionWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
    CoordFrame->setBackgroundRole(QPalette::Base);
    RSTFrame->setBackgroundRole(QPalette::Base);
}

LinksWidget::LinksWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

DatabaseWidget::DatabaseWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

LogWidget::LogWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
}
