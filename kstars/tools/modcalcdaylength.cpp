/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcdaylength.h"

#include "geolocation.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/kssun.h"
#include "skyobjects/skyobject.h"

#include <KLineEdit>

modCalcDayLength::modCalcDayLength(QWidget *parentSplit) : QFrame(parentSplit)
{
    setupUi(this);

    showCurrentDate();
    initGeo();
    slotComputeAlmanac();

    connect(Date, SIGNAL(dateChanged(QDate)), this, SLOT(slotComputeAlmanac()));
    connect(Location, SIGNAL(clicked()), this, SLOT(slotLocation()));

    connect(LocationBatch, SIGNAL(clicked()), this, SLOT(slotLocationBatch()));
    connect(InputFileBatch, SIGNAL(urlSelected(QUrl)), this, SLOT(slotCheckFiles()));
    connect(OutputFileBatch, SIGNAL(urlSelected(QUrl)), this, SLOT(slotCheckFiles()));
    connect(RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));
    connect(ViewButtonBatch, SIGNAL(clicked()), this, SLOT(slotViewBatch()));

    RunButtonBatch->setEnabled(false);
    ViewButtonBatch->setEnabled(false);

    show();
}

void modCalcDayLength::showCurrentDate(void)
{
    KStarsDateTime dt(KStarsDateTime::currentDateTime());
    Date->setDate(dt.date());
}

void modCalcDayLength::initGeo(void)
{
    KStarsData *data = KStarsData::Instance();
    geoPlace         = data->geo();
    geoBatch         = data->geo();
    Location->setText(geoPlace->fullName());
    LocationBatch->setText(geoBatch->fullName());
}

QTime modCalcDayLength::lengthOfDay(const QTime &setQTime, const QTime &riseQTime)
{
    QTime dL(0, 0, 0);
    int dds       = riseQTime.secsTo(setQTime);
    QTime dLength = dL.addSecs(dds);

    return dLength;
}

void modCalcDayLength::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geoPlace = newGeo;
            Location->setText(geoPlace->fullName());
        }
    }
    delete ld;

    slotComputeAlmanac();
}

void modCalcDayLength::slotLocationBatch()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geoBatch = newGeo;
            LocationBatch->setText(geoBatch->fullName());
        }
    }
    delete ld;
}

void modCalcDayLength::updateAlmanac(const QDate &d, GeoLocation *geo)
{
    //Determine values needed for the Almanac
    long double jd0 = KStarsDateTime(d, QTime(8, 0, 0)).djd();
    KSNumbers num(jd0);

    //Sun
    KSSun Sun;
    Sun.findPosition(&num);

    QTime ssTime = Sun.riseSetTime(KStarsDateTime(jd0), geo, false);
    QTime srTime = Sun.riseSetTime(KStarsDateTime(jd0), geo, true);
    QTime stTime = Sun.transitTime(KStarsDateTime(jd0), geo);

    dms ssAz  = Sun.riseSetTimeAz(KStarsDateTime(jd0), geo, false);
    dms srAz  = Sun.riseSetTimeAz(KStarsDateTime(jd0), geo, true);
    dms stAlt = Sun.transitAltitude(KStarsDateTime(jd0), geo);

    //In most cases, the Sun will rise and set:
    if (ssTime.isValid())
    {
        ssAzString  = ssAz.toDMSString();
        stAltString = stAlt.toDMSString();
        srAzString  = srAz.toDMSString();

        ssTimeString = QLocale().toString(ssTime, "hh:mm:ss");
        srTimeString = QLocale().toString(srTime, "hh:mm:ss");
        stTimeString = QLocale().toString(stTime, "hh:mm:ss");

        QTime daylength = lengthOfDay(ssTime, srTime);
        //daylengthString = QLocale().toString(daylength);
        daylengthString = QLocale().toString(daylength, "hh:mm:ss");

        //...but not always!
    }
    else if (stAlt.Degrees() > 0.)
    {
        ssAzString  = i18n("Circumpolar");
        stAltString = stAlt.toDMSString();
        srAzString  = i18n("Circumpolar");

        ssTimeString    = "--:--";
        srTimeString    = "--:--";
        stTimeString    = QLocale().toString(stTime, "hh:mm:ss");
        daylengthString = "24:00";
    }
    else if (stAlt.Degrees() < 0.)
    {
        ssAzString  = i18n("Does not rise");
        stAltString = stAlt.toDMSString();
        srAzString  = i18n("Does not set");

        ssTimeString    = "--:--";
        srTimeString    = "--:--";
        stTimeString    = QLocale().toString(stTime, "hh:mm:ss");
        daylengthString = "00:00";
    }

    //Moon
    KSMoon Moon;

    QTime msTime = Moon.riseSetTime(KStarsDateTime(jd0), geo, false);
    QTime mrTime = Moon.riseSetTime(KStarsDateTime(jd0), geo, true);
    QTime mtTime = Moon.transitTime(KStarsDateTime(jd0), geo);

    dms msAz  = Moon.riseSetTimeAz(KStarsDateTime(jd0), geo, false);
    dms mrAz  = Moon.riseSetTimeAz(KStarsDateTime(jd0), geo, true);
    dms mtAlt = Moon.transitAltitude(KStarsDateTime(jd0), geo);

    //In most cases, the Moon will rise and set:
    if (msTime.isValid())
    {
        msAzString  = msAz.toDMSString();
        mtAltString = mtAlt.toDMSString();
        mrAzString  = mrAz.toDMSString();

        msTimeString = QLocale().toString(msTime, "hh:mm:ss");
        mrTimeString = QLocale().toString(mrTime, "hh:mm:ss");
        mtTimeString = QLocale().toString(mtTime, "hh:mm:ss");

        //...but not always!
    }
    else if (mtAlt.Degrees() > 0.)
    {
        msAzString  = i18n("Circumpolar");
        mtAltString = mtAlt.toDMSString();
        mrAzString  = i18n("Circumpolar");

        msTimeString = "--:--";
        mrTimeString = "--:--";
        mtTimeString = QLocale().toString(mtTime, "hh:mm:ss");
    }
    else if (mtAlt.Degrees() < 0.)
    {
        msAzString  = i18n("Does not rise");
        mtAltString = mtAlt.toDMSString();
        mrAzString  = i18n("Does not rise");

        msTimeString = "--:--";
        mrTimeString = "--:--";
        mtTimeString = QLocale().toString(mtTime, "hh:mm:ss");
    }

    //after calling riseSetTime Phase needs to reset, setting it before causes Phase to set nan
    Moon.findPosition(&num);
    Moon.findPhase(nullptr);
    lunarphaseString = Moon.phaseName() + " (" + QString::number(int(100 * Moon.illum())) + "%)";

    //Fix length of Az strings
    if (srAz.Degrees() < 100.0)
        srAzString = ' ' + srAzString;
    if (ssAz.Degrees() < 100.0)
        ssAzString = ' ' + ssAzString;
    if (mrAz.Degrees() < 100.0)
        mrAzString = ' ' + mrAzString;
    if (msAz.Degrees() < 100.0)
        msAzString = ' ' + msAzString;
}

void modCalcDayLength::slotComputeAlmanac()
{
    updateAlmanac(Date->date(), geoPlace);

    SunSet->setText(ssTimeString);
    SunRise->setText(srTimeString);
    SunTransit->setText(stTimeString);
    SunSetAz->setText(ssAzString);
    SunRiseAz->setText(srAzString);
    SunTransitAlt->setText(stAltString);
    DayLength->setText(daylengthString);

    MoonSet->setText(msTimeString);
    MoonRise->setText(mrTimeString);
    MoonTransit->setText(mtTimeString);
    MoonSetAz->setText(msAzString);
    MoonRiseAz->setText(mrAzString);
    MoonTransitAlt->setText(mtAltString);
    LunarPhase->setText(lunarphaseString);
}

void modCalcDayLength::slotCheckFiles()
{
    bool flag = !InputFileBatch->lineEdit()->text().isEmpty() && !OutputFileBatch->lineEdit()->text().isEmpty();
    RunButtonBatch->setEnabled(flag);
}

void modCalcDayLength::slotRunBatch()
{
    QString inputFileName = InputFileBatch->url().toLocalFile();

    if (QFile::exists(inputFileName))
    {
        QFile f(inputFileName);
        if (!f.open(QIODevice::ReadOnly))
        {
            QString message = i18n("Could not open file %1.", f.fileName());
            KSNotification::sorry(message, i18n("Could Not Open File"));
            return;
        }

        QTextStream istream(&f);
        processLines(istream);
        ViewButtonBatch->setEnabled(true);

        f.close();
    }
    else
    {
        QString message = i18n("Invalid file: %1", inputFileName);
        KSNotification::sorry(message, i18n("Invalid file"));
        return;
    }
}

void modCalcDayLength::processLines(QTextStream &istream)
{
    QFile fOut(OutputFileBatch->url().toLocalFile());
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    //Write header
    ostream << "# " << i18nc("%1 is a location on earth", "Almanac for %1", geoBatch->fullName())
            << QString("  [%1, %2]").arg(geoBatch->lng()->toDMSString(), geoBatch->lat()->toDMSString())
            << "\n# " << i18n("computed by KStars") << endl
            << "# " << "SRise: Sun Rise" << endl
            << "# " << "STran: Sun Transit" << endl
            << "# " << "SSet: Sun Set" << endl
            << "# " << "SRiseAz: Azimuth of Sun Rise" << endl
            << "# " << "STranAlt: Altitude of Sun Transit" << endl
            << "# " << "SSetAz: Azimuth of Sun Set" << endl
            << "# " << "STranAlt: Altitude of Sun Transit" << endl
            << "# " << "DayLen: Day Duration in hours" << endl
            << "# " << "MRise: Moon Rise" << endl
            << "# " << "MTran: Moon Transit" << endl
            << "# " << "MSet: Moon Set" << endl
            << "# " << "MRiseAz: Azimuth of Moon Rise" << endl
            << "# " << "MTranAkt: Altitude of Moon Transit" << endl
            << "# " << "MSetAz: Azimuth of Moon Set" << endl
            << "# " << "LunarPhase: Lunar Phase and Illumination Percentage" << endl
            << "# " << endl
            << "# Date,SRise,STran,SSet,SRiseAz,STranAlt,SSetAz,DayLen,MRise,MTran,MSet,"
            << "MRiseAz,MTranAlt,MSetAz,LunarPhase" << endl
            << "#" << endl;

    QString line;
    QDate d;

    while (!istream.atEnd())
    {
        line = istream.readLine();
        line = line.trimmed();

        //Parse the line as a date, then compute Almanac values
        d = QDate::fromString(line, Qt::ISODate);
        if (d.isValid())
        {
            updateAlmanac(d, geoBatch);
            ostream << d.toString(Qt::ISODate) << "," <<
                    srTimeString << "," <<
                    stTimeString << "," <<
                    ssTimeString << "," <<
                    srAzString << "," <<
                    stAltString << "," <<
                    ssAzString << "," <<
                    daylengthString << "," <<
                    mrTimeString << "," <<
                    mtTimeString << "," <<
                    msTimeString << "," <<
                    mrAzString << "," <<
                    mtAltString << "," <<
                    msAzString << "," <<
                    lunarphaseString << endl;
        }
    }
}

void modCalcDayLength::slotViewBatch()
{
    QFile fOut(OutputFileBatch->url().toLocalFile());
    fOut.open(QIODevice::ReadOnly);
    QTextStream istream(&fOut);
    QStringList text;

    while (!istream.atEnd())
        text.append(istream.readLine());

    fOut.close();

    KMessageBox::informationList(nullptr, i18n("Results of Almanac calculation"), text, OutputFileBatch->url().toLocalFile());
}
