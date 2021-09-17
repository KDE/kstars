/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcgeodcoord.h"

#include "dms.h"
#include "geolocation.h"
#include "kstars.h"
#include "ksnotification.h"
#include "kstarsdata.h"

#include <QTextStream>
#include <QFileDialog>

modCalcGeodCoord::modCalcGeodCoord(QWidget *parentSplit) : QFrame(parentSplit)
{
    QStringList ellipsoidList;

    ellipsoidList << "IAU76"
                  << "GRS80"
                  << "MERIT83"
                  << "WGS84"
                  << "IERS89";

    setupUi(this);

    spheRadio->setChecked(true);
    ellipsoidBox->insertItems(5, ellipsoidList);
    geoPlace.reset(new GeoLocation(dms(0), dms(0)));
    showLongLat();
    setEllipsoid(0);
    show();

    connect(Clear, SIGNAL(clicked()), this, SLOT(slotClearGeoCoords()));
    connect(Compute, SIGNAL(clicked()), this, SLOT(slotComputeGeoCoords()));
}

void modCalcGeodCoord::showLongLat(void)
{
    KStarsData *data = KStarsData::Instance();

    LongGeoBox->show(data->geo()->lng());
    LatGeoBox->show(data->geo()->lat());
    AltGeoBox->setText(QString("0.0"));
}

void modCalcGeodCoord::setEllipsoid(int index)
{
    geoPlace->changeEllipsoid(index);
}

void modCalcGeodCoord::getCartGeoCoords(void)
{
    geoPlace->setXPos(XGeoBox->text().toDouble() * 1000.);
    geoPlace->setYPos(YGeoBox->text().toDouble() * 1000.);
    geoPlace->setZPos(ZGeoBox->text().toDouble() * 1000.);
}

void modCalcGeodCoord::getSphGeoCoords(void)
{
    geoPlace->setLong(LongGeoBox->createDms());
    geoPlace->setLat(LatGeoBox->createDms());
    geoPlace->setElevation(AltGeoBox->text().toDouble());
}

void modCalcGeodCoord::slotClearGeoCoords(void)
{
    geoPlace->setLong(dms(0.0));
    geoPlace->setLat(dms(0.0));
    geoPlace->setElevation(0.0);
    LatGeoBox->clearFields();
    LongGeoBox->clearFields();
}

void modCalcGeodCoord::slotComputeGeoCoords(void)
{
    if (cartRadio->isChecked())
    {
        getCartGeoCoords();
        showSpheGeoCoords();
    }
    else
    {
        getSphGeoCoords();
        showCartGeoCoords();
    }
}

void modCalcGeodCoord::showSpheGeoCoords(void)
{
    LongGeoBox->show(geoPlace->lng());
    LatGeoBox->show(geoPlace->lat());
    AltGeoBox->setText(QLocale().toString(geoPlace->elevation(), 3));
}

void modCalcGeodCoord::showCartGeoCoords(void)
{
    XGeoBox->setText(QLocale().toString(geoPlace->xPos() / 1000., 6));
    YGeoBox->setText(QLocale().toString(geoPlace->yPos() / 1000., 6));
    ZGeoBox->setText(QLocale().toString(geoPlace->zPos() / 1000., 6));
}

void modCalcGeodCoord::geoCheck(void)
{
    XGeoBoxBatch->setEnabled(false);
    XGeoCheckBatch->setChecked(false);
    YGeoBoxBatch->setEnabled(false);
    YGeoCheckBatch->setChecked(false);
    YGeoBoxBatch->setEnabled(false);
    YGeoCheckBatch->setChecked(false);
    xyzInputCoords = false;
}

void modCalcGeodCoord::xyzCheck(void)
{
    LongGeoBoxBatch->setEnabled(false);
    LongGeoCheckBatch->setChecked(false);
    LatGeoBoxBatch->setEnabled(false);
    LatGeoCheckBatch->setChecked(false);
    AltGeoBoxBatch->setEnabled(false);
    AltGeoCheckBatch->setChecked(false);
    xyzInputCoords = true;
}

void modCalcGeodCoord::slotLongCheckedBatch()
{
    if (LongGeoCheckBatch->isChecked())
    {
        LongGeoBoxBatch->setEnabled(false);
        geoCheck();
    }
    else
    {
        LongGeoBoxBatch->setEnabled(true);
    }
}

void modCalcGeodCoord::slotLatCheckedBatch()
{
    if (LatGeoCheckBatch->isChecked())
    {
        LatGeoBoxBatch->setEnabled(false);
        geoCheck();
    }
    else
    {
        LatGeoBoxBatch->setEnabled(true);
    }
}

void modCalcGeodCoord::slotElevCheckedBatch()
{
    if (AltGeoCheckBatch->isChecked())
    {
        AltGeoBoxBatch->setEnabled(false);
        geoCheck();
    }
    else
    {
        AltGeoBoxBatch->setEnabled(true);
    }
}

void modCalcGeodCoord::slotXCheckedBatch()
{
    if (XGeoCheckBatch->isChecked())
    {
        XGeoBoxBatch->setEnabled(false);
        xyzCheck();
    }
    else
    {
        XGeoBoxBatch->setEnabled(true);
    }
}

void modCalcGeodCoord::slotYCheckedBatch()
{
    if (YGeoCheckBatch->isChecked())
    {
        YGeoBoxBatch->setEnabled(false);
        xyzCheck();
    }
    else
    {
        YGeoBoxBatch->setEnabled(true);
    }
}

void modCalcGeodCoord::slotZCheckedBatch()
{
    if (ZGeoCheckBatch->isChecked())
    {
        ZGeoBoxBatch->setEnabled(false);
        xyzCheck();
    }
    else
    {
        ZGeoBoxBatch->setEnabled(true);
    }
}
void modCalcGeodCoord::slotInputFile()
{
    const QString inputFileName = QFileDialog::getOpenFileName(KStars::Instance(), QString(), QString());
    if (!inputFileName.isEmpty())
        InputFileBoxBatch->setUrl(QUrl::fromLocalFile(inputFileName));
}

void modCalcGeodCoord::slotOutputFile()
{
    const QString outputFileName = QFileDialog::getSaveFileName();
    if (!outputFileName.isEmpty())
        OutputFileBoxBatch->setUrl(QUrl::fromLocalFile(outputFileName));
}

void modCalcGeodCoord::slotRunBatch(void)
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

        //		processLines(&f);
        QTextStream istream(&f);
        processLines(istream);
        //		readFile( istream );
        f.close();
    }
    else
    {
        QString message = i18n("Invalid file: %1", inputFileName);
        KSNotification::sorry(message, i18n("Invalid file"));
        InputFileBoxBatch->setUrl(QUrl());
    }
}

void modCalcGeodCoord::processLines(QTextStream &istream)
{
    // we open the output file

    //	QTextStream istream(&fIn);
    const QString outputFileName = OutputFileBoxBatch->url().toLocalFile();
    QFile fOut(outputFileName);
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    QString line;
    QChar space = ' ';
    int i       = 0;
    GeoLocation geoPl(dms(0), dms(0));
    geoPl.setEllipsoid(0);

    double xB, yB, zB, hB;
    dms latB, longB;

    while (!istream.atEnd())
    {
        line = istream.readLine();
        line = line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split(' ');

        i = 0;

        // Input coords are XYZ:

        if (xyzInputCoords)
        {
            // Read X and write in ostream if corresponds

            if (XGeoCheckBatch->isChecked())
            {
                xB = fields[i].toDouble();
                i++;
            }
            else
                xB = XGeoBoxBatch->text().toDouble();

            if (AllRadioBatch->isChecked())
                ostream << xB << space;
            else if (XGeoCheckBatch->isChecked())
                ostream << xB << space;

            // Read Y and write in ostream if corresponds

            if (YGeoCheckBatch->isChecked())
            {
                yB = fields[i].toDouble();
                i++;
            }
            else
                yB = YGeoBoxBatch->text().toDouble();

            if (AllRadioBatch->isChecked())
                ostream << yB << space;
            else if (YGeoCheckBatch->isChecked())
                ostream << yB << space;
            // Read Z and write in ostream if corresponds

            if (ZGeoCheckBatch->isChecked())
            {
                zB = fields[i].toDouble();
                i++;
            }
            else
                zB = ZGeoBoxBatch->text().toDouble();

            if (AllRadioBatch->isChecked())
                ostream << zB << space;
            else if (YGeoCheckBatch->isChecked())
                ostream << zB << space;

            geoPl.setXPos(xB * 1000.0);
            geoPl.setYPos(yB * 1000.0);
            geoPl.setZPos(zB * 1000.0);
            ostream << geoPl.lng()->toDMSString() << space << geoPl.lat()->toDMSString() << space << geoPl.elevation()
                    << '\n';

            // Input coords. are Long, Lat and Height
        }
        else
        {
            // Read Longitude and write in ostream if corresponds

            if (LongGeoCheckBatch->isChecked())
            {
                longB = dms::fromString(fields[i], true);
                i++;
            }
            else
                longB = LongGeoBoxBatch->createDms(true);

            if (AllRadioBatch->isChecked())
                ostream << longB.toDMSString() << space;
            else if (LongGeoCheckBatch->isChecked())
                ostream << longB.toDMSString() << space;

            // Read Latitude and write in ostream if corresponds

            if (LatGeoCheckBatch->isChecked())
            {
                latB = dms::fromString(fields[i], true);
                i++;
            }
            else
                latB = LatGeoBoxBatch->createDms(true);

            if (AllRadioBatch->isChecked())
                ostream << latB.toDMSString() << space;
            else if (LatGeoCheckBatch->isChecked())
                ostream << latB.toDMSString() << space;

            // Read Height and write in ostream if corresponds

            if (AltGeoCheckBatch->isChecked())
            {
                hB = fields[i].toDouble();
                i++;
            }
            else
                hB = AltGeoBoxBatch->text().toDouble();

            if (AllRadioBatch->isChecked())
                ostream << hB << space;
            else if (AltGeoCheckBatch->isChecked())
                ostream << hB << space;

            geoPl.setLong(longB);
            geoPl.setLat(latB);
            geoPl.setElevation(hB);

            ostream << geoPl.xPos() / 1000.0 << space << geoPl.yPos() / 1000.0 << space << geoPl.zPos() / 1000.0
                    << '\n';
        }
    }

    fOut.close();
}
