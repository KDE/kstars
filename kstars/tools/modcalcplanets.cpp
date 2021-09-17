/*
    SPDX-FileCopyrightText: 2004-2005 Pablo de Vicente <p.devicentea@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcplanets.h"

#include "geolocation.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/kssun.h"

modCalcPlanets::modCalcPlanets(QWidget *parentSplit) : QFrame(parentSplit)
{
    setupUi(this);

    KStarsDateTime dt(KStarsDateTime::currentDateTime());

    DateTimeBox->setDateTime(dt);
    DateBoxBatch->setDate(dt.date());
    UTBoxBatch->setTime(dt.time());

    geoPlace = KStarsData::Instance()->geo();
    LocationButton->setText(geoPlace->fullName());

    RABox->setDegType(false);

    // signals and slots connections
    connect(PlanetComboBox, SIGNAL(activated(int)), this, SLOT(slotComputePosition()));
    connect(DateTimeBox, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(slotComputePosition()));
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));

    connect(UTCheckBatch, SIGNAL(clicked()), this, SLOT(slotUtCheckedBatch()));
    connect(DateCheckBatch, SIGNAL(clicked()), this, SLOT(slotDateCheckedBatch()));
    connect(LatCheckBatch, SIGNAL(clicked()), this, SLOT(slotLatCheckedBatch()));
    connect(LongCheckBatch, SIGNAL(clicked()), this, SLOT(slotLongCheckedBatch()));
    connect(PlanetCheckBatch, SIGNAL(clicked()), this, SLOT(slotPlanetsCheckedBatch()));

    slotComputePosition();
    show();
}

void modCalcPlanets::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);

    if (ld->exec() == QDialog::Accepted)
    {
        geoPlace = ld->selectedCity();
        LocationButton->setText(geoPlace->fullName());
        slotComputePosition();
    }
    delete ld;
}

void modCalcPlanets::slotComputePosition()
{
    KStarsDateTime dt(DateTimeBox->dateTime());
    long double julianDay = dt.djd();
    KSNumbers num(julianDay);
    CachingDms LST(geoPlace->GSTtoLST(dt.gst()));

    // Earth
    KSPlanet Earth(i18n("Earth"));
    Earth.findPosition(&num);

    // Earth is special case!
    if (PlanetComboBox->currentIndex() == 2)
    {
        showCoordinates(Earth);
        return;
    }

    // Pointer to hold planet data. Pointer is used since it has to
    // hold objects of different type. It's safe to use new/delete
    // because exceptions are disallowed.
    std::unique_ptr<KSPlanetBase> p;

    switch (PlanetComboBox->currentIndex())
    {
        case 0:
            p.reset(new KSPlanet(KSPlanetBase::MERCURY));
            break;
        case 1:
            p.reset(new KSPlanet(KSPlanetBase::VENUS));
            break;
        case 3:
            p.reset(new KSPlanet(KSPlanetBase::MARS));
            break;
        case 4:
            p.reset(new KSPlanet(KSPlanetBase::JUPITER));
            break;
        case 5:
            p.reset(new KSPlanet(KSPlanetBase::SATURN));
            break;
        case 6:
            p.reset(new KSPlanet(KSPlanetBase::URANUS));
            break;
        case 7:
            p.reset(new KSPlanet(KSPlanetBase::NEPTUNE));
            break;
        /*case 8:
            p.reset(new KSPluto(); break;*/
        case 8:
            p.reset(new KSMoon());
            break;
        case 9:
            p.reset(new KSSun());
            p->setRsun(0.0);
            break;
    }
    if (p.get() == nullptr)
        return;

    // Show data.
    p->findPosition(&num, geoPlace->lat(), &LST, &Earth);
    p->EquatorialToHorizontal(&LST, geoPlace->lat());
    showCoordinates(*p);
}

void modCalcPlanets::showCoordinates(const KSPlanetBase &ksp)
{
    showHeliocentricEclipticCoords(ksp.helEcLong(), ksp.helEcLat(), ksp.rsun());
    showGeocentricEclipticCoords(ksp.ecLong(), ksp.ecLat(), ksp.rearth());
    showEquatorialCoords(ksp.ra(), ksp.dec());
    showTopocentricCoords(ksp.az(), ksp.alt());
}

void modCalcPlanets::showHeliocentricEclipticCoords(const dms &hLong, const dms &hLat, double dist)
{
    HelioLongBox->show(hLong);
    HelioLatBox->show(hLat);
    HelioDistBox->setText(QLocale().toString(dist, 6));
}

void modCalcPlanets::showGeocentricEclipticCoords(const dms &eLong, const dms &eLat, double dist)
{
    GeoLongBox->show(eLong);
    GeoLatBox->show(eLat);
    GeoDistBox->setText(QLocale().toString(dist, 6));
}

void modCalcPlanets::showEquatorialCoords(const dms &ra, const dms &dec)
{
    RABox->show(ra, false);
    DecBox->show(dec);
}

void modCalcPlanets::showTopocentricCoords(const dms &az, const dms &el)
{
    AzBox->show(az);
    AltBox->show(el);
}

void modCalcPlanets::slotPlanetsCheckedBatch()
{
    PlanetComboBoxBatch->setEnabled(!PlanetCheckBatch->isChecked());
}

void modCalcPlanets::slotUtCheckedBatch()
{
    UTBoxBatch->setEnabled(!UTCheckBatch->isChecked());
}

void modCalcPlanets::slotDateCheckedBatch()
{
    DateBoxBatch->setEnabled(!DateCheckBatch->isChecked());
}

void modCalcPlanets::slotLongCheckedBatch()
{
    LongBoxBatch->setEnabled(!LongCheckBatch->isChecked());
}

void modCalcPlanets::slotLatCheckedBatch()
{
    LatBoxBatch->setEnabled(!LatCheckBatch->isChecked());
}

void modCalcPlanets::slotRunBatch()
{
    const QString inputFileName = InputFileBoxBatch->url().toLocalFile();

    // We open the input file and read its content

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
        f.close();
    }
    else
    {
        QString message = i18n("Invalid file: %1", inputFileName);
        KSNotification::sorry(message, i18n("Invalid file"));
        InputFileBoxBatch->setUrl(QUrl());
    }
}

unsigned int modCalcPlanets::requiredBatchFields()
{
    unsigned int i = 0;

    if (PlanetCheckBatch->isChecked())
        i++;
    if (UTCheckBatch->isChecked())
        i++;
    if (DateCheckBatch->isChecked())
        i++;
    if (LongCheckBatch->isChecked())
        i++;
    if (LatCheckBatch->isChecked())
        i++;

    return i;
}

void modCalcPlanets::processLines(QTextStream &istream)
{
    // we open the output file

    const QString outputFileName = OutputFileBoxBatch->url().toLocalFile();
    QFile fOut(outputFileName);
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);
    bool lineIsValid = true;

    QChar space = ' ';
    QString planetB;
    unsigned int i = 0, nline = 0;
    QTime utB;
    QDate dtB;
    CachingDms longB, latB, hlongB, hlatB, glongB, glatB, raB, decB, azmB, altB;
    double rSunB(0.0), rEarthB(0.0);

    //Initialize planet names
    QString pn;
    QStringList pNames, pNamesi18n;
    pNames << "Mercury"
           << "Venus"
           << "Earth"
           << "Mars"
           << "Jupiter"
           << "Saturn"
           << "Uranus"
           << "Neptune" /* << "Pluto" */
           << "Sun"
           << "Moon";
    pNamesi18n << i18n("Mercury") << i18n("Venus") << i18n("Earth") << i18n("Mars") << i18n("Jupiter") << i18n("Saturn")
               << i18n("Uranus") << i18n("Neptune") /* << i18nc("Asteroid name (optional)", "Pluto") */
               << i18n("Sun") << i18n("Moon");

    ///Parse the input file
    int numberOfRequiredFields = requiredBatchFields();
    while (!istream.atEnd())
    {
        QString lineToWrite;
        QString line = istream.readLine();
        line = line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split(' ');

        if (fields.count() != numberOfRequiredFields)
        {
            lineIsValid = false;
            qWarning() << i18n("Incorrect number of fields in line %1: ", nline)
                       << i18n("Present fields %1. ", fields.count())
                       << i18n("Required fields %1. ", numberOfRequiredFields);
            nline++;
            continue;
        }

        i = 0;
        if (PlanetCheckBatch->isChecked())
        {
            planetB = fields[i];
            int j   = pNamesi18n.indexOf(planetB);
            if (j == -1)
            {
                qWarning() << i18n("Unknown planet ") << fields[i] << i18n(" in line %1: ", nline);
                continue;
            }
            pn = pNames.at(j); //untranslated planet name
            i++;
        }
        else
        {
            planetB = PlanetComboBoxBatch->currentText();
        }
        if (AllRadioBatch->isChecked() || PlanetCheckBatch->isChecked())
        {
            lineToWrite = planetB;
            lineToWrite += space;
        }

        // Read Ut and write in ostream if corresponds
        if (UTCheckBatch->isChecked())
        {
            utB = QTime::fromString(fields[i]);
            if (!utB.isValid())
            {
                qWarning() << i18n("Line %1 contains an invalid time", nline);
                lineIsValid = false;
                nline++;
                continue;
            }
            i++;
        }
        else
        {
            utB = UTBoxBatch->time();
        }
        if (AllRadioBatch->isChecked() || UTCheckBatch->isChecked())
            lineToWrite += QLocale().toString(utB).append(space);

        // Read date and write in ostream if corresponds
        if (DateCheckBatch->isChecked())
        {
            dtB = QDate::fromString(fields[i], Qt::ISODate);
            if (!dtB.isValid())
            {
                qWarning() << i18n("Line %1 contains an invalid date: ", nline) << fields[i];
                lineIsValid = false;
                nline++;
                continue;
            }
            i++;
        }
        else
        {
            dtB = DateBoxBatch->date();
        }
        if (AllRadioBatch->isChecked() || DateCheckBatch->isChecked())
            lineToWrite += QLocale().toString(dtB, QLocale::LongFormat).append(space);

        // Read Longitude and write in ostream if corresponds

        if (LongCheckBatch->isChecked())
        {
            longB = CachingDms::fromString(fields[i], true);
            i++;
        }
        else
        {
            longB = LongBoxBatch->createDms(true);
        }
        if (AllRadioBatch->isChecked() || LongCheckBatch->isChecked())
            lineToWrite += longB.toDMSString() + space;

        // Read Latitude
        if (LatCheckBatch->isChecked())
        {
            latB = CachingDms::fromString(fields[i], true);
            i++;
        }
        else
        {
            latB = LatBoxBatch->createDms(true);
        }
        if (AllRadioBatch->isChecked() || LatCheckBatch->isChecked())
            lineToWrite += latB.toDMSString() + space;

        KStarsDateTime edt(dtB, utB);
        CachingDms LST = edt.gst() + longB;

        KSNumbers num(edt.djd());
        KSPlanet Earth(i18n("Earth"));
        Earth.findPosition(&num);

        // FIXME: allocate new object for every iteration is probably not wisest idea.
        KSPlanetBase *kspb = nullptr;
        /*if ( pn == "Pluto" ) {
            kspb = new KSPluto();
        } else*/
        if (pn == i18n("Sun"))
        {
            kspb = new KSSun();
        }
        else if (pn == i18n("Moon"))
        {
            kspb = new KSMoon();
        }
        else
        {
            kspb = new KSPlanet(i18n(pn.toLocal8Bit()), QString(), Qt::white, 1.0);
        }
        kspb->findPosition(&num, &latB, &LST, &Earth);
        kspb->EquatorialToHorizontal(&LST, &latB);

        // Heliocentric Ecl. coords.
        hlongB = kspb->helEcLong();
        hlatB  = kspb->helEcLat();
        rSunB  = kspb->rsun();
        // Geocentric Ecl. coords.
        glongB  = kspb->ecLong();
        glatB   = kspb->ecLat();
        rEarthB = kspb->rearth();
        // Equatorial coords.
        decB = kspb->dec();
        raB  = kspb->ra();
        // Topocentric Coords.
        azmB = kspb->az();
        altB = kspb->alt();

        ostream << lineToWrite;

        if (HelioEclCheckBatch->isChecked())
            ostream << hlongB.toDMSString() << space << hlatB.toDMSString() << space << rSunB << space;
        if (GeoEclCheckBatch->isChecked())
            ostream << glongB.toDMSString() << space << glatB.toDMSString() << space << rEarthB << space;
        if (EquatorialCheckBatch->isChecked())
            ostream << raB.toHMSString() << space << decB.toDMSString() << space;
        if (HorizontalCheckBatch->isChecked())
            ostream << azmB.toDMSString() << space << altB.toDMSString() << space;
        ostream << '\n';

        // Delete object
        delete kspb;

        nline++;
    }

    if (!lineIsValid)
    {
        QString message = i18n("Errors found while parsing some lines in the input file");
        KSNotification::sorry(message, i18n("Errors in lines"));
    }

    fOut.close();
}
