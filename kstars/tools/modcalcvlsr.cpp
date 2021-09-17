/*
    SPDX-FileCopyrightText: 2005 Pablo de Vicente <p.devicente@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcvlsr.h"

#include "dms.h"
#include "geolocation.h"
#include "ksnumbers.h"
#include "kstars.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "dialogs/locationdialog.h"
#include "dialogs/finddialog.h"
#include "skyobjects/skypoint.h"
#include "widgets/dmsbox.h"

#include <KLocalizedString>

#include <QPointer>
#include <QFileDialog>

modCalcVlsr::modCalcVlsr(QWidget *parentSplit) : QFrame(parentSplit), velocityFlag(0)
{
    setupUi(this);
    RA->setDegType(false);

    Date->setDateTime(KStarsDateTime::currentDateTime());
    initGeo();

    VLSR->setValidator(new QDoubleValidator(VLSR));
    VHelio->setValidator(new QDoubleValidator(VHelio));
    VGeo->setValidator(new QDoubleValidator(VGeo));
    VTopo->setValidator(new QDoubleValidator(VTopo));

    // signals and slots connections
    connect(Date, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(slotCompute()));
    connect(NowButton, SIGNAL(clicked()), this, SLOT(slotNow()));
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(ObjectButton, SIGNAL(clicked()), this, SLOT(slotFindObject()));
    connect(RA, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Dec, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(VLSR, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(VHelio, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(VGeo, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(VTopo, SIGNAL(editingFinished()), this, SLOT(slotCompute()));

    connect(RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));

    show();
}

void modCalcVlsr::initGeo(void)
{
    geoPlace = KStarsData::Instance()->geo();
    LocationButton->setText(geoPlace->fullName());
}

void modCalcVlsr::slotNow()
{
    Date->setDateTime(KStarsDateTime::currentDateTime());
    slotCompute();
}

void modCalcVlsr::slotFindObject()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *o = FindDialog::Instance()->targetObject();
        RA->showInHours(o->ra0());
        Dec->showInDegrees(o->dec0());
    }
}

void modCalcVlsr::slotLocation()
{
    QPointer<LocationDialog> ld(new LocationDialog(this));

    if (ld->exec() == QDialog::Accepted && ld)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geoPlace = newGeo;
            LocationButton->setText(geoPlace->fullName());
        }
    }
    delete ld;

    slotCompute();
}

void modCalcVlsr::slotCompute()
{
    bool ok1(false), ok2(false);
    SkyPoint sp(RA->createDms(false, &ok1), Dec->createDms(true, &ok2));
    if (!ok1 || !ok2)
        return;

    KStarsDateTime dt(Date->dateTime());
    double vst[3];

    geoPlace->TopocentricVelocity(vst, dt.gst());

    if (sender()->objectName() == "VLSR")
        velocityFlag = 0;
    if (sender()->objectName() == "VHelio")
        velocityFlag = 1;
    if (sender()->objectName() == "VGeo")
        velocityFlag = 2;
    if (sender()->objectName() == "VTopo")
        velocityFlag = 3;

    switch (velocityFlag)
    {
        case 0: //Hold VLSR constant, compute the others
        {
            double vlsr   = VLSR->text().toDouble();
            double vhelio = sp.vHeliocentric(vlsr, dt.djd());
            double vgeo   = sp.vGeocentric(vhelio, dt.djd());

            VHelio->setText(QString::number(vhelio));
            VGeo->setText(QString::number(vgeo));
            VTopo->setText(QString::number(sp.vTopocentric(vgeo, vst)));
            break;
        }

        case 1: //Hold VHelio constant, compute the others
        {
            double vhelio = VHelio->text().toDouble();
            double vlsr   = sp.vHelioToVlsr(vhelio, dt.djd());
            double vgeo   = sp.vGeocentric(vhelio, dt.djd());

            VLSR->setText(QString::number(vlsr));
            VGeo->setText(QString::number(vgeo));
            VTopo->setText(QString::number(sp.vTopocentric(vgeo, vst)));
            break;
        }

        case 2: //Hold VGeo constant, compute the others
        {
            double vgeo   = VGeo->text().toDouble();
            double vhelio = sp.vGeoToVHelio(vgeo, dt.djd());
            double vlsr   = sp.vHelioToVlsr(vhelio, dt.djd());

            VLSR->setText(QString::number(vlsr));
            VHelio->setText(QString::number(vhelio));
            VTopo->setText(QString::number(sp.vTopocentric(vgeo, vst)));
            break;
        }

        case 3: //Hold VTopo constant, compute the others
        {
            double vtopo  = VTopo->text().toDouble();
            double vgeo   = sp.vTopoToVGeo(vtopo, vst);
            double vhelio = sp.vGeoToVHelio(vgeo, dt.djd());
            double vlsr   = sp.vHelioToVlsr(vhelio, dt.djd());

            VLSR->setText(QString::number(vlsr));
            VHelio->setText(QString::number(vhelio));
            VGeo->setText(QString::number(vgeo));
            break;
        }

        default: //oops
            qDebug() << "Error: do not know which velocity to use for input.";
            break;
    }
}

void modCalcVlsr::slotUtChecked()
{
    if (UTCheckBatch->isChecked())
        UTBoxBatch->setEnabled(false);
    else
    {
        UTBoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotDateChecked()
{
    if (DateCheckBatch->isChecked())
        DateBoxBatch->setEnabled(false);
    else
    {
        DateBoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotRaChecked()
{
    if (RACheckBatch->isChecked())
    {
        RABoxBatch->setEnabled(false);
    }
    else
    {
        RABoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotDecChecked()
{
    if (DecCheckBatch->isChecked())
    {
        DecBoxBatch->setEnabled(false);
    }
    else
    {
        DecBoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotEpochChecked()
{
    if (EpochCheckBatch->isChecked())
        EpochBoxBatch->setEnabled(false);
    else
        EpochBoxBatch->setEnabled(true);
}

void modCalcVlsr::slotLongChecked()
{
    if (LongCheckBatch->isChecked())
        LongitudeBoxBatch->setEnabled(false);
    else
        LongitudeBoxBatch->setEnabled(true);
}

void modCalcVlsr::slotLatChecked()
{
    if (LatCheckBatch->isChecked())
        LatitudeBoxBatch->setEnabled(false);
    else
    {
        LatitudeBoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotHeightChecked()
{
    if (ElevationCheckBatch->isChecked())
        ElevationBoxBatch->setEnabled(false);
    else
    {
        ElevationBoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotVlsrChecked()
{
    if (InputVelocityCheckBatch->isChecked())
        InputVelocityBoxBatch->setEnabled(false);
    else
    {
        InputVelocityBoxBatch->setEnabled(true);
    }
}

void modCalcVlsr::slotInputFile()
{
    const QString inputFileName = QFileDialog::getOpenFileName(KStars::Instance(), QString(), QString());
    if (!inputFileName.isEmpty())
        InputFileBoxBatch->setUrl(QUrl::fromLocalFile(inputFileName));
}

void modCalcVlsr::slotOutputFile()
{
    const QString outputFileName = QFileDialog::getSaveFileName();
    if (!outputFileName.isEmpty())
        OutputFileBoxBatch->setUrl(QUrl::fromLocalFile(outputFileName));
}

void modCalcVlsr::slotRunBatch()
{
    const QString inputFileName = InputFileBoxBatch->url().toLocalFile();

    // We open the input file and read its content

    if (QFile::exists(inputFileName))
    {
        QFile f(inputFileName);
        if (!f.open(QIODevice::ReadOnly))
        {
            KSNotification::sorry(i18n("Could not open file %1.", f.fileName()), i18n("Could Not Open File"));
            return;
        }

        //		processLines(&f);
        QTextStream istream(&f);
        processLines(istream);
        //		readFile( istream );
        f.close();
    }
    else
    {
        KSNotification::sorry(i18n("Invalid file: %1", inputFileName), i18n("Invalid file"));
        InputFileBoxBatch->setUrl(QUrl());
    }
}

void modCalcVlsr::processLines(QTextStream &istream)
{
    // we open the output file

    //	QTextStream istream(&fIn);
    QString outputFileName;
    outputFileName = OutputFileBoxBatch->url().toLocalFile();
    QFile fOut(outputFileName);
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    QString line;
    QChar space = ' ';
    int i       = 0;
    long double jd0;
    SkyPoint spB;
    double sra, cra, sdc, cdc;
    dms raB, decB, latB, longB;
    QString epoch0B;
    double vhB, vgB, vtB, vlsrB, heightB;
    double vtopo[3];
    QTime utB;
    QDate dtB;
    KStarsDateTime dt0B;

    while (!istream.atEnd())
    {
        line = istream.readLine();
        line = line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split(' ');

        i = 0;

        // Read Ut and write in ostream if corresponds

        if (UTCheckBatch->isChecked())
        {
            utB = QTime::fromString(fields[i]);
            i++;
        }
        else
            utB = UTBoxBatch->time();

        if (AllRadioBatch->isChecked())
            ostream << QLocale().toString(utB) << space;
        else if (UTCheckBatch->isChecked())
            ostream << QLocale().toString(utB) << space;

        // Read date and write in ostream if corresponds

        if (DateCheckBatch->isChecked())
        {
            dtB = QDate::fromString(fields[i]);
            i++;
        }
        else
            dtB = DateBoxBatch->date();
        if (AllRadioBatch->isChecked())
            ostream << QLocale().toString(dtB, QLocale::LongFormat).append(space);
        else if (DateCheckBatch->isChecked())
            ostream << QLocale().toString(dtB, QLocale::LongFormat).append(space);

        // Read RA and write in ostream if corresponds

        if (RACheckBatch->isChecked())
        {
            raB = dms::fromString(fields[i], false);
            i++;
        }
        else
            raB = RABoxBatch->createDms(false);

        if (AllRadioBatch->isChecked())
            ostream << raB.toHMSString() << space;
        else if (RACheckBatch->isChecked())
            ostream << raB.toHMSString() << space;

        // Read DEC and write in ostream if corresponds

        if (DecCheckBatch->isChecked())
        {
            decB = dms::fromString(fields[i], true);
            i++;
        }
        else
            decB = DecBoxBatch->createDms();

        if (AllRadioBatch->isChecked())
            ostream << decB.toDMSString() << space;
        else if (DecCheckBatch->isChecked())
            ostream << decB.toDMSString() << space;

        // Read Epoch and write in ostream if corresponds

        if (EpochCheckBatch->isChecked())
        {
            epoch0B = fields[i];
            i++;
        }
        else
            epoch0B = EpochBoxBatch->text();

        if (AllRadioBatch->isChecked())
            ostream << epoch0B << space;
        else if (EpochCheckBatch->isChecked())
            ostream << epoch0B << space;

        // Read vlsr and write in ostream if corresponds

        if (InputVelocityCheckBatch->isChecked())
        {
            vlsrB = fields[i].toDouble();
            i++;
        }
        else
            vlsrB = InputVelocityComboBatch->currentText().toDouble();

        if (AllRadioBatch->isChecked())
            ostream << vlsrB << space;
        else if (InputVelocityCheckBatch->isChecked())
            ostream << vlsrB << space;

        // Read Longitude and write in ostream if corresponds

        if (LongCheckBatch->isChecked())
        {
            longB = dms::fromString(fields[i], true);
            i++;
        }
        else
            longB = LongitudeBoxBatch->createDms(true);

        if (AllRadioBatch->isChecked())
            ostream << longB.toDMSString() << space;
        else if (LongCheckBatch->isChecked())
            ostream << longB.toDMSString() << space;

        // Read Latitude

        if (LatCheckBatch->isChecked())
        {
            latB = dms::fromString(fields[i], true);
            i++;
        }
        else
            latB = LatitudeBoxBatch->createDms(true);
        if (AllRadioBatch->isChecked())
            ostream << latB.toDMSString() << space;
        else if (LatCheckBatch->isChecked())
            ostream << latB.toDMSString() << space;

        // Read height and write in ostream if corresponds

        if (ElevationCheckBatch->isChecked())
        {
            heightB = fields[i].toDouble();
            i++;
        }
        else
            heightB = ElevationBoxBatch->text().toDouble();

        if (AllRadioBatch->isChecked())
            ostream << heightB << space;
        else if (ElevationCheckBatch->isChecked())
            ostream << heightB << space;

        // We make the first calculations

        spB = SkyPoint(raB, decB);
        dt0B.setFromEpoch(epoch0B);
        vhB = spB.vHeliocentric(vlsrB, dt0B.djd());
        jd0 = KStarsDateTime(dtB, utB).djd();
        vgB = spB.vGeocentric(vlsrB, jd0);
        geoPlace->setLong(longB);
        geoPlace->setLat(latB);
        geoPlace->setElevation(heightB);
        dms gsidt = KStarsDateTime(dtB, utB).gst();
        geoPlace->TopocentricVelocity(vtopo, gsidt);
        spB.ra().SinCos(sra, cra);
        spB.dec().SinCos(sdc, cdc);
        vtB = vgB - (vtopo[0] * cdc * cra + vtopo[1] * cdc * sra + vtopo[2] * sdc);

        ostream << vhB << space << vgB << space << vtB << '\n';
    }

    fOut.close();
}
