/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "detaildialoglite.h"

#include "constellationboundarylines.h"
#include "deepskyobject.h"
#include "kspaths.h"
#include "ksutils.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "skymaplite.h"
#include "starobject.h"
#include "kstarslite/skyobjectlite.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/supernova.h"

#include <QDesktopServices>
#include <QTemporaryFile>

DetailDialogLite::DetailDialogLite()
{
    setProperty("isLinksOn", true);
    setProperty("isLogOn", true);
}

void DetailDialogLite::initialize()
{
    connect(SkyMapLite::Instance(), SIGNAL(objectLiteChanged()), this, SLOT(createGeneralTab()));
    connect(SkyMapLite::Instance(), SIGNAL(objectLiteChanged()), this, SLOT(createPositionTab()));
    connect(SkyMapLite::Instance(), SIGNAL(objectLiteChanged()), this, SLOT(createLogTab()));
    connect(SkyMapLite::Instance(), SIGNAL(objectLiteChanged()), this, SLOT(createLinksTab()));
}

void DetailDialogLite::createGeneralTab()
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    // Stuff that should be visible only for specific types of objects
    setProperty("illumination", ""); // Only shown for the moon
    setProperty("BVindex", "");      // Only shown for stars
    setupThumbnail();

    //Fill in the data fields
    //Contents depend on type of object
    QString objecttyp, str;

    switch (selectedObject->type())
    {
        case SkyObject::STAR:
        {
            StarObject *s = (StarObject *)selectedObject;

            if (s->getHDIndex())
            {
                setProperty("name", (QString("%1, HD %2").arg(s->longname()).arg(s->getHDIndex())));
            }
            else
            {
                setProperty("name", s->longname());
            }

            objecttyp = s->sptype() + ' ' + i18n("star");
            setProperty("magnitude", i18nc("number in magnitudes", "%1 mag",
                                           QLocale().toString(s->mag(), 'f', 2))); //show to hundredth place

            if (s->getBVIndex() < 30.)
            {
                setProperty("BVindex", QString::number(s->getBVIndex(), 'f', 2));
            }

            //distance
            if (s->distance() > 2000. || s->distance() < 0.) // parallax < 0.5 mas
            {
                setProperty("distance", (QString(i18nc("larger than 2000 parsecs", "> 2000 pc"))));
            }
            else if (s->distance() > 50.) //show to nearest integer
            {
                setProperty("distance",
                            (i18nc("number in parsecs", "%1 pc", QLocale().toString(s->distance(), 'f', 0))));
            }
            else if (s->distance() > 10.0) //show to tenths place
            {
                setProperty("distance",
                            (i18nc("number in parsecs", "%1 pc", QLocale().toString(s->distance(), 'f', 1))));
            }
            else //show to hundredths place
            {
                setProperty("distance",
                            (i18nc("number in parsecs", "%1 pc", QLocale().toString(s->distance(), 'f', 2))));
            }

            //Note multiplicity/variability in angular size label
            setProperty("angSize", QString());
            if (s->isMultiple() && s->isVariable())
            {
                QString multiple = QString(i18nc("the star is a multiple star", "multiple") + ',');
                setProperty("angSize", QString(multiple + '\n' + (i18nc("the star is a variable star", "variable"))));
            }
            else if (s->isMultiple())
            {
                setProperty("angSize", i18nc("the star is a multiple star", "multiple"));
            }
            else if (s->isVariable())
            {
                setProperty("angSize", (i18nc("the star is a variable star", "variable")));
            }

            break; //end of stars case
        }
        case SkyObject::ASTEROID: //[fall through to planets]
        case SkyObject::COMET:    //[fall through to planets]
        case SkyObject::MOON:     //[fall through to planets]
        case SkyObject::PLANET:
        {
            KSPlanetBase *ps = (KSPlanetBase *)selectedObject;

            setProperty("name", ps->longname());

            //Type is "G5 star" for Sun
            if (ps->name() == "Sun")
            {
                objecttyp = i18n("G5 star");
            }
            else if (ps->name() == "Moon")
            {
                objecttyp = ps->translatedName();
            }
            else if (ps->name() == i18n("Pluto") || ps->name() == "Ceres" ||
                     ps->name() == "Eris") // TODO: Check if Ceres / Eris have translations and i18n() them
            {
                objecttyp = i18n("Dwarf planet");
            }
            else
            {
                objecttyp = ps->typeName();
            }

            //The moon displays illumination fraction and updateMag is called to calculate moon's current magnitude
            if (selectedObject->name() == "Moon")
            {
                setProperty(
                    "illumination",
                    (QString("%1 %").arg(QLocale().toString(((KSMoon *)selectedObject)->illum() * 100., 'f', 0))));
                ((KSMoon *)selectedObject)->updateMag();
            }

            // JM: Shouldn't we use the calculated magnitude? Disabling the following
            /*
                    if(selectedObject->type() == SkyObject::COMET){
                        Data->Magnitude->setText(i18nc("number in magnitudes", "%1 mag",
                                                 QLocale().toString( ((KSComet *)selectedObject)->getTotalMagnitudeParameter(), 'f', 2)));  //show to hundredth place

                    }
                    else{*/
            setProperty("magnitude", (i18nc("number in magnitudes", "%1 mag",
                                            QLocale().toString(ps->mag(), 'f', 2)))); //show to hundredth place
            //}

            //Distance from Earth.  The moon requires a unit conversion
            if (ps->name() == "Moon")
            {
                setProperty("distance", (i18nc("distance in kilometers", "%1 km",
                                               QLocale().toString(ps->rearth() * AU_KM, 'f', 2))));
            }
            else
            {
                setProperty("distance", (i18nc("distance in Astronomical Units", "%1 AU",
                                               QLocale().toString(ps->rearth(), 'f', 3))));
            }

            //Angular size; moon and sun in arcmin, others in arcsec
            if (ps->angSize())
            {
                if (ps->name() == "Sun" || ps->name() == "Moon")
                {
                    setProperty(
                        "angSize",
                        (i18nc(
                            "angular size in arcminutes", "%1 arcmin",
                            QLocale().toString(
                                ps->angSize(), 'f',
                                1)))); // Needn't be a plural form because sun / moon will never contract to 1 arcminute
                }
                else
                {
                    setProperty("angSize", i18nc("angular size in arcseconds", "%1 arcsec",
                                                 QLocale().toString(ps->angSize() * 60.0, 'f', 1)));
                }
            }
            else
            {
                setProperty("angSize", "--");
            }

            break; //end of planets/comets/asteroids case
        }
        case SkyObject::SUPERNOVA:
        {
            Supernova *sup = (Supernova *)selectedObject;

            objecttyp = i18n("Supernova");
            setProperty("name", sup->name());
            setProperty("magnitude", (i18nc("number in magnitudes", "%1 mag", QLocale().toString(sup->mag(), 'f', 2))));
            setProperty("distance", "---");
            break;
        }
        default: //deep-sky objects
        {
            DeepSkyObject *dso = (DeepSkyObject *)selectedObject;

            //Show all names recorded for the object
            QStringList nameList;
            if (!dso->longname().isEmpty() && dso->longname() != dso->name())
            {
                nameList.append(dso->translatedLongName());
                nameList.append(dso->translatedName());
            }
            else
            {
                nameList.append(dso->translatedName());
            }

            if (!dso->translatedName2().isEmpty())
            {
                nameList.append(dso->translatedName2());
            }

            if (dso->ugc() != 0)
            {
                nameList.append(QString("UGC %1").arg(dso->ugc()));
            }

            if (dso->pgc() != 0)
            {
                nameList.append(QString("PGC %1").arg(dso->pgc()));
            }

            setProperty("name", nameList.join(","));

            objecttyp = dso->typeName();

            if (dso->type() == SkyObject::RADIO_SOURCE)
            {
                //ta->MagLabel->setText(i18nc("integrated flux at a frequency", "Flux(%1):", dso->customCatalog()->fluxFrequency()));
                //Data->Magnitude->setText(i18nc("integrated flux value", "%1 %2",
                //                             QLocale().toString(dso->flux(), 'f', 1), dso->customCatalog()->fluxUnit()));  //show to tenths place
            }
            else if (dso->mag() > 90.0)
            {
                setProperty("magnitude", "--");
            }
            else
            {
                setProperty("magnitude", i18nc("number in magnitudes", "%1 mag",
                                               QLocale().toString(dso->mag(), 'f', 1))); //show to tenths place
            }

            //No distances at this point...
            setProperty("distance", "--");

            //Only show decimal place for small angular sizes
            if (dso->a() > 10.0)
            {
                setProperty("angSize",
                            i18nc("angular size in arcminutes", "%1 arcmin", QLocale().toString(dso->a(), 'f', 0)));
            }
            else if (dso->a())
            {
                setProperty("angSize",
                            i18nc("angular size in arcminutes", "%1 arcmin", QLocale().toString(dso->a(), 'f', 1)));
            }
            else
            {
                setProperty("angSize", "--");
            }

            break;
        }
    }

    //Reset advanced properties
    setProperty("perihilion", "");
    setProperty("orbitID", "");
    setProperty("NEO", "");
    setProperty("diameter", "");
    setProperty("rotation", "");
    setProperty("earthMOID", "");
    setProperty("orbitClass", "");
    setProperty("albedo", "");
    setProperty("dimensions", "");
    setProperty("period", "");

    // Add specifics data
    switch (selectedObject->type())
    {
        case SkyObject::ASTEROID:
        {
            KSAsteroid *ast = (KSAsteroid *)selectedObject;

            // Perihelion
            str.setNum(ast->getPerihelion());
            setProperty("perihelion", QString(str + " AU"));
            // Earth MOID
            if (ast->getEarthMOID() == 0)
                str = "";
            else
                str.setNum(ast->getEarthMOID()).append(" AU");
            setProperty("earthMOID", str);
            // Orbit ID
            setProperty("orbitID", ast->getOrbitID());
            // Orbit Class
            setProperty("orbitClass", ast->getOrbitClass());
            // NEO
            if (ast->isNEO())
                setProperty("NEO", "Yes");
            else
                setProperty("NEO", "No");
            // Albedo
            if (ast->getAlbedo() == 0.0)
                str = "";
            else
                str.setNum(ast->getAlbedo());
            setProperty("albedo", str);
            // Diameter
            if (ast->getDiameter() == 0.0)
                str = "";
            else
                str.setNum(ast->getDiameter()).append(" km");
            setProperty("diameter", str);
            // Dimensions
            if (ast->getDimensions().isEmpty())
                setProperty("dimensions", "");
            else
                setProperty("dimensions", QString(ast->getDimensions() + " km"));
            // Rotation period
            if (ast->getRotationPeriod() == 0.0)
                str = "";
            else
                str.setNum(ast->getRotationPeriod()).append(" h");
            setProperty("rotation", str);
            // Period
            if (ast->getPeriod() == 0.0)
                str = "";
            else
                str.setNum(ast->getPeriod()).append(" y");
            setProperty("period", str);

            break;
        }
        case SkyObject::COMET:
        {
            KSComet *com = (KSComet *)selectedObject;

            // Perihelion
            str.setNum(com->getPerihelion());
            setProperty("perihelion", QString(str + " AU"));
            // Earth MOID
            if (com->getEarthMOID() == 0)
                str = "";
            else
                str.setNum(com->getEarthMOID()).append(" AU");
            setProperty("earthMOID", str);
            // Orbit ID
            setProperty("orbitID", com->getOrbitID());
            // Orbit Class
            setProperty("orbitClass", com->getOrbitClass());
            // NEO
            if (com->isNEO())
                setProperty("NEO", "Yes");
            else
                setProperty("NEO", "No");
            // Albedo
            if (com->getAlbedo() == 0.0)
                str = "";
            else
                str.setNum(com->getAlbedo());
            setProperty("albedo", str);
            // Diameter
            if (com->getDiameter() == 0.0)
                str = "";
            else
                str.setNum(com->getDiameter()).append(" km");
            setProperty("diameter", str);
            // Dimensions
            if (com->getDimensions().isEmpty())
                setProperty("dimensions", "");
            else
                setProperty("dimensions", QString(com->getDimensions() + " km"));
            // Rotation period
            if (com->getRotationPeriod() == 0.0)
                str = "";
            else
                str.setNum(com->getRotationPeriod()).append(" h");
            setProperty("rotation", str);
            // Period
            if (com->getPeriod() == 0.0)
                str = "";
            else
                str.setNum(com->getPeriod()).append(" y");
            setProperty("period", str);

            break;
        }
    }

    //Common to all types:
    QString cname = KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(selectedObject);
    if (selectedObject->type() != SkyObject::CONSTELLATION)
    {
        cname = i18nc("%1 type of sky object (planet, asteroid etc), %2 name of a constellation", "%1 in %2", objecttyp,
                      cname);
    }
    setProperty("typeInConstellation", cname);
}

void DetailDialogLite::createPositionTab()
{
    KStarsData *data  = KStarsData::Instance();
    KStarsDateTime ut = data->ut();
    GeoLocation *geo  = data->geo();

    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    //Coordinates Section:
    //Don't use KLocale::formatNumber() for the epoch string,
    //because we don't want a thousands-place separator!
    QString sEpoch = QString::number(ut.epoch(), 'f', 1);
    //Replace the decimal point with localized decimal symbol
    sEpoch.replace('.', QLocale().decimalPoint());

    qDebug() << (selectedObject->deprecess(data->updateNum(), 2451545.0l)).ra0().toHMSString()
             << (selectedObject->deprecess(data->updateNum(), 2451545.0l)).dec0().toDMSString() << endl;
    //qDebug() << selectedObject->ra().toHMSString() << selectedObject->dec().toDMSString() << endl;
    setProperty("RALabel", i18n("RA (%1):", sEpoch));
    setProperty("decLabel", i18n("Dec (%1):", sEpoch));
    setProperty("RA", selectedObject->ra().toHMSString());
    setProperty("dec", selectedObject->dec().toDMSString());

    selectedObject->EquatorialToHorizontal(data->lst(), data->geo()->lat());

    setProperty("az", selectedObject->az().toDMSString());
    dms a;

    if (Options::useAltAz())
        a = selectedObject->alt();
    else
        a = selectedObject->altRefracted();
    setProperty("alt", a.toDMSString());

    // Display the RA0 and Dec0 for objects that are outside the solar system
    if (!selectedObject->isSolarSystem())
    {
        setProperty("RA0", selectedObject->ra0().toHMSString());
        setProperty("dec0", selectedObject->dec0().toDMSString());
    }
    else
    {
        setProperty("RA0", "--");
        setProperty("dec0", "--");
    }

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
    setProperty("HA", QString("%1%2").arg(sgn).arg(ha.toHMSString()));

    //Airmass is approximated as the secant of the zenith distance,
    //equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
    if (selectedObject->alt().Degrees() > 0.0)
        setProperty("airmass", QLocale().toString(selectedObject->airmass(), 'f', 2));
    else
        setProperty("airmass", "--");

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

    //If set time is before rise time, use set time for tomorrow
    QTime st = selectedObject->riseSetTime(ut, geo, false);   //false = use set time
    dms saz  = selectedObject->riseSetTimeAz(ut, geo, false); //false = use set time
    if (st < rt)
    {
        st  = selectedObject->riseSetTime(ut.addDays(1), geo, false);   //false = use set time
        saz = selectedObject->riseSetTimeAz(ut.addDays(1), geo, false); //false = use set time
    }

    if (rt.isValid())
    {
        setProperty("timeRise", QString().sprintf("%02d:%02d", rt.hour(), rt.minute()));
        setProperty("timeSet", QString().sprintf("%02d:%02d", st.hour(), st.minute()));
        setProperty("azRise", raz.toDMSString());
        setProperty("azSet", saz.toDMSString());
    }
    else
    {
        if (selectedObject->alt().Degrees() > 0.0)
        {
            setProperty("timeRise", i18n("Circumpolar"));
            setProperty("timeSet", i18n("Circumpolar"));
        }
        else
        {
            setProperty("timeRise", i18n("Never rises"));
            setProperty("timeSet", i18n("Never rises"));
        }

        setProperty("azRise", i18nc("Not Applicable", "N/A"));
        setProperty("azSet", i18nc("Not Applicable", "N/A"));
    }

    setProperty("timeTransit", QString().sprintf("%02d:%02d", tt.hour(), tt.minute()));
    setProperty("altTransit", talt.toDMSString());

    // Restore the position and other time-dependent parameters
    selectedObject->recomputeCoords(ut, geo);
}

void DetailDialogLite::createLogTab()
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();
    //Don't create a log tab for an unnamed star
    if (selectedObject->name() == QString("star"))
    {
        setProperty("isLogOn", false);
        return;
    }
    setProperty("isLogOn", true);

    if (selectedObject->userLog().isEmpty())
    {
        setProperty("userLog",
                    i18n("Record here observation logs and/or data on %1.", selectedObject->translatedName()));
    }
    else
    {
        setProperty("userLog", selectedObject->userLog());
    }

    /*//Automatically save the log contents when the widget loses focus
    connect( Log->UserLog, SIGNAL(focusOut()), this, SLOT(saveLogData()) );*/
}

void DetailDialogLite::createLinksTab()
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    //No links for unnamed stars
    if (selectedObject->name() == QString("star"))
    {
        setProperty("isLinksOn", false);
        return;
    }

    setProperty("isLinksOn", true);

    QStringList newInfoList;
    foreach (const QString &s, selectedObject->InfoTitle())
        newInfoList.append(i18nc("Image/info menu item (should be translated)", s.toLocal8Bit()));
    setProperty("infoTitleList", newInfoList);

    QStringList newImageList;
    foreach (const QString &s, selectedObject->ImageTitle())
        newImageList.append(i18nc("Image/info menu item (should be translated)", s.toLocal8Bit()));

    setProperty("imageTitleList", newImageList);
}

void DetailDialogLite::updateLocalDatabase(int type, const QString &search_line, const QString &replace_line)
{
    QString TempFileName, file_line;
    QFile URLFile;
    QTemporaryFile TempFile;
    QTextStream *temp_stream = nullptr;
    QTextStream *out_stream  = nullptr;
    bool replace             = !replace_line.isEmpty();

    if (search_line.isEmpty())
        return;

    TempFile.setAutoRemove(false);
    TempFile.open();
    TempFileName = TempFile.fileName();

    switch (type)
    {
        // Info Links
        case 0:
            // Get name for our local info_url file
            URLFile.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("info_url.dat"));
            break;

        // Image Links
        case 1:
            // Get name for our local info_url file
            URLFile.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("image_url.dat"));
            break;
    }

    if (!URLFile.open(QIODevice::ReadWrite))
    {
        qDebug() << "DetailDialog: Failed to open " << URLFile.fileName();
        qDebug() << "KStars cannot save to user database";
        return;
    }

    // Copy URL file to temp file
    TempFile.write(URLFile.readAll());
    //Return pointers to initial positions
    TempFile.seek(0);
    //Clear URLFile
    URLFile.resize(0);

    // Get streams;
    temp_stream = new QTextStream(&TempFile);
    out_stream  = new QTextStream(&URLFile);

    while (!temp_stream->atEnd())
    {
        file_line = temp_stream->readLine();
        // If we find a match, either replace, or remove (by skipping).
        if (file_line == search_line)
        {
            if (replace)
                (*out_stream) << replace_line << endl;
            else
                continue;
        }
        else
            (*out_stream) << file_line << endl;
    }

    URLFile.close();
    delete temp_stream;
    delete out_stream;
}

void DetailDialogLite::addLink(const QString &url, const QString &desc, bool isImageLink)
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    if (url.isEmpty() || desc.isEmpty())
        return; //Do nothing if empty url or desc were provided

    QString entry;
    QFile file;

    if (isImageLink)
    {
        //Add link to object's ImageList, and descriptive text to its ImageTitle list
        selectedObject->ImageList().append(url);
        selectedObject->ImageTitle().append(desc);

        //Also, update the user's custom image links database
        //check for user's image-links database.  If it doesn't exist, create it.
        //determine filename in local user KDE directory tree.
        file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("image_url.dat"));

        if (!file.open(QIODevice::ReadWrite | QIODevice::Append))
        {
            QString message =
                i18n("Custom image-links file could not be opened.\nLink cannot be recorded for future sessions.");
            qDebug() << message;
            return;
        }
        else
        {
            entry = selectedObject->name() + ':' + desc + ':' + url;
            QTextStream stream(&file);
            stream << entry << endl;
            file.close();
            setProperty("imageTitleList", selectedObject->ImageTitle());
        }
    }
    else
    {
        selectedObject->InfoList().append(url);
        selectedObject->InfoTitle().append(desc);

        //check for user's image-links database.  If it doesn't exist, create it.
        //determine filename in local user KDE directory tree.
        file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("info_url.dat"));

        if (!file.open(QIODevice::ReadWrite | QIODevice::Append))
        {
            QString message = i18n(
                "Custom information-links file could not be opened.\nLink cannot be recorded for future sessions.");
            qDebug() << message;
            return;
        }
        else
        {
            entry = selectedObject->name() + ':' + desc + ':' + url;
            QTextStream stream(&file);
            stream << entry << endl;
            file.close();
            setProperty("infoTitleList", selectedObject->InfoTitle());
        }
    }
}

void DetailDialogLite::removeLink(int itemIndex, bool isImage)
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    QString currentItemURL, currentItemTitle, LineEntry, TempFileName, FileLine;
    QFile URLFile;
    QTemporaryFile TempFile;
    TempFile.setAutoRemove(false);
    TempFile.open();
    TempFileName = TempFile.fileName();

    //Check if it is a valid index
    if (itemIndex < 0)
    {
        return;
    }
    else if (isImage && itemIndex >= selectedObject->ImageTitle().length())
    {
        return;
    }
    else if (!isImage && itemIndex >= selectedObject->InfoTitle().length())
    {
        return;
    }
    //if (title.isEmpty() || url.isEmpty()) return;

    if (!isImage) //Information
    {
        currentItemTitle = selectedObject->InfoTitle()[itemIndex];
        currentItemURL   = selectedObject->InfoList()[itemIndex];
        LineEntry        = selectedObject->name();
        LineEntry += ':';
        LineEntry += currentItemTitle;
        LineEntry += ':';
        LineEntry += currentItemURL;
    }
    else //Image
    {
        currentItemTitle = selectedObject->ImageTitle()[itemIndex];
        currentItemURL   = selectedObject->ImageList()[itemIndex];
        LineEntry        = selectedObject->name();
        LineEntry += ':';
        LineEntry += currentItemTitle;
        LineEntry += ':';
        LineEntry += currentItemURL;
    }

    /*if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove the %1 link?", currentItemTitle), i18n("Delete Confirmation"),KStandardGuiItem::del())!=KMessageBox::Continue)
        return;*/

    if (isImage)
    {
        selectedObject->ImageTitle().removeAt(itemIndex);
        selectedObject->ImageList().removeAt(itemIndex);
    }
    else
    {
        selectedObject->InfoTitle().removeAt(itemIndex);
        selectedObject->InfoList().removeAt(itemIndex);
    }

    // Remove link from file
    updateLocalDatabase(isImage ? 1 : 0, LineEntry);

    setProperty("infoTitleList", selectedObject->InfoTitle());
    setProperty("imageTitleList", selectedObject->ImageTitle());
}

void DetailDialogLite::editLink(int itemIndex, bool isImage, const QString &desc, const QString &url)
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    if (url.isEmpty() || desc.isEmpty())
        return; //Do nothing if empty url or desc were provided

    QString search_line, replace_line, currentItemTitle, currentItemURL;

    //Check if it is a valid index
    if (itemIndex < 0)
    {
        return;
    }
    else if (isImage && itemIndex >= selectedObject->ImageTitle().length())
    {
        return;
    }
    else if (!isImage && itemIndex >= selectedObject->InfoTitle().length())
    {
        return;
    }

    if (!isImage) //Information
    {
        currentItemTitle = selectedObject->InfoTitle()[itemIndex];
        currentItemURL   = selectedObject->InfoList()[itemIndex];
        search_line      = selectedObject->name();
        search_line += ':';
        search_line += currentItemTitle;
        search_line += ':';
        search_line += currentItemURL;
    }
    else //Image
    {
        currentItemTitle = selectedObject->ImageTitle()[itemIndex];
        currentItemURL   = selectedObject->ImageList()[itemIndex];
        search_line      = selectedObject->name();
        search_line += ':';
        search_line += currentItemTitle;
        search_line += ':';
        search_line += currentItemURL;
    }

    bool go(true);

    // If nothing changed, skip th action
    if (url == currentItemURL && desc == currentItemTitle)
        go = false;

    if (go)
    {
        replace_line = selectedObject->name() + ':' + desc + ':' + url;

        // Info Link
        if (!isImage)
        {
            selectedObject->InfoTitle().replace(itemIndex, desc);
            selectedObject->InfoList().replace(itemIndex, url);

            // Image Links
        }
        else
        {
            selectedObject->ImageTitle().replace(itemIndex, desc);
            selectedObject->ImageList().replace(itemIndex, url);
        }

        // Update local files
        updateLocalDatabase(isImage ? 1 : 0, search_line, replace_line);

        setProperty("infoTitleList", selectedObject->InfoTitle());
        setProperty("imageTitleList", selectedObject->ImageTitle());
    }
}

QString DetailDialogLite::getInfoURL(int index)
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();
    QStringList urls          = selectedObject->InfoList();
    if (index >= 0 && index < urls.size())
    {
        return urls[index];
    }
    else
    {
        return "";
    }
}

QString DetailDialogLite::getImageURL(int index)
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();
    QStringList urls          = selectedObject->ImageList();
    if (index >= 0 && index < urls.size())
    {
        return urls[index];
    }
    else
    {
        return "";
    }
}

void DetailDialogLite::setupThumbnail()
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();
    //No image if object is a star
    if (selectedObject->type() == SkyObject::STAR || selectedObject->type() == SkyObject::CATALOG_STAR)
    {
        /*Thumbnail->scaled( Data->Image->width(), Data->Image->height() );
        Thumbnail->fill( Data->DataFrame->palette().color( QPalette::Window ) );
        Data->Image->setPixmap( *Thumbnail );*/
        setProperty("thumbnail", "");
        return;
    }

    //Try to load the object's image from disk
    //If no image found, load "no image" image
    QFile file;
    QString fname = "thumb-" + selectedObject->name().toLower().remove(' ') + ".png";
    if (KSUtils::openDataFile(file, fname))
    {
        file.close();
        setProperty("thumbnail", file.fileName());
    }
    else
    {
        setProperty("thumbnail", "");
    }
}

/*void DetailDialogLite::viewResource(int itemIndex, bool isImage) {
    QString url;
    if(isImage) {
        url = getImageURL(itemIndex);
    } else {
        url = getInfoURL(itemIndex);
    }
    QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
}*/

void DetailDialogLite::saveLogData(const QString &userLog)
{
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    selectedObject->saveUserLog(userLog);
}
