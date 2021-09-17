/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcgalcoord.h"

#include "ksnotification.h"
#include "dialogs/finddialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/skypoint.h"

#include <QTextStream>
#include <QPointer>

modCalcGalCoord::modCalcGalCoord(QWidget *parentSplit) : QFrame(parentSplit)
{
    setupUi(this);
    RA->setDegType(false);

    connect(RA, SIGNAL(editingFinished()), this, SLOT(slotComputeCoords()));
    connect(Dec, SIGNAL(editingFinished()), this, SLOT(slotComputeCoords()));
    connect(GalLongitude, SIGNAL(editingFinished()), this, SLOT(slotComputeCoords()));
    connect(GalLatitude, SIGNAL(editingFinished()), this, SLOT(slotComputeCoords()));

    connect(ObjectButton, SIGNAL(clicked()), this, SLOT(slotObject()));

    connect(decCheckBatch, SIGNAL(clicked()), this, SLOT(slotDecCheckedBatch()));
    connect(raCheckBatch, SIGNAL(clicked()), this, SLOT(slotRaCheckedBatch()));
    connect(epochCheckBatch, SIGNAL(clicked()), this, SLOT(slotEpochCheckedBatch()));
    connect(galLongCheckBatch, SIGNAL(clicked()), this, SLOT(slotGalLongCheckedBatch()));
    connect(galLatCheckBatch, SIGNAL(clicked()), this, SLOT(slotGalLatCheckedBatch()));
    connect(runButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));

    show();
}

void modCalcGalCoord::slotObject()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *o = FindDialog::Instance()->targetObject();
        RA->showInHours(o->ra());
        Dec->showInDegrees(o->dec());
        slotComputeCoords();
    }
}

void modCalcGalCoord::slotComputeCoords()
{
    if (GalLongitude->hasFocus())
        GalLongitude->clearFocus();

    //Determine whether we should compute galactic coords from
    //equatorial, or vice versa
    if (sender()->objectName() == "GalLongitude" || sender()->objectName() == "GalLatitude")
    {
        //Validate GLong and GLat
        bool ok(false);
        dms glat;
        dms glong = GalLongitude->createDms(true, &ok);
        if (ok)
            glat = GalLatitude->createDms(true, &ok);
        if (ok)
        {
            SkyPoint sp;
            sp.GalacticToEquatorial1950(&glong, &glat);
            sp.B1950ToJ2000();
            RA->showInHours(sp.ra());
            Dec->showInDegrees(sp.dec());
        }
    }
    else
    {
        //Validate RA and Dec
        bool ok(false);
        dms dec;
        dms ra = RA->createDms(false, &ok);
        if (ok)
            dec = Dec->createDms(true, &ok);
        if (ok)
        {
            dms glong, glat;
            SkyPoint sp(ra, dec);
            sp.J2000ToB1950();
            sp.Equatorial1950ToGalactic(glong, glat);
            GalLongitude->showInDegrees(glong);
            GalLatitude->showInDegrees(glat);
        }
    }
}

void modCalcGalCoord::galCheck()
{
    galLatCheckBatch->setChecked(false);
    galLatBoxBatch->setEnabled(false);
    galLongCheckBatch->setChecked(false);
    galLongBoxBatch->setEnabled(false);
    galInputCoords = false;
}

void modCalcGalCoord::equCheck()
{
    raCheckBatch->setChecked(false);
    raBoxBatch->setEnabled(false);
    decCheckBatch->setChecked(false);
    decBoxBatch->setEnabled(false);
    epochCheckBatch->setChecked(false);
    galInputCoords = true;
}

void modCalcGalCoord::slotRaCheckedBatch()
{
    if (raCheckBatch->isChecked())
    {
        raBoxBatch->setEnabled(false);
        galCheck();
    }
    else
    {
        raBoxBatch->setEnabled(true);
    }
}

void modCalcGalCoord::slotDecCheckedBatch()
{
    if (decCheckBatch->isChecked())
    {
        decBoxBatch->setEnabled(false);
        galCheck();
    }
    else
    {
        decBoxBatch->setEnabled(true);
    }
}

void modCalcGalCoord::slotEpochCheckedBatch()
{
    epochCheckBatch->setChecked(false);

    if (epochCheckBatch->isChecked())
    {
        epochBoxBatch->setEnabled(false);
        galCheck();
    }
    else
    {
        epochBoxBatch->setEnabled(true);
    }
}

void modCalcGalCoord::slotGalLatCheckedBatch()
{
    if (galLatCheckBatch->isChecked())
    {
        galLatBoxBatch->setEnabled(false);
        equCheck();
    }
    else
    {
        galLatBoxBatch->setEnabled(true);
    }
}

void modCalcGalCoord::slotGalLongCheckedBatch()
{
    if (galLongCheckBatch->isChecked())
    {
        galLongBoxBatch->setEnabled(false);
        equCheck();
    }
    else
    {
        galLongBoxBatch->setEnabled(true);
    }
}

void modCalcGalCoord::slotRunBatch()
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
        QString message = i18n("Invalid file: %1", inputFileName);
        KSNotification::sorry(message, i18n("Invalid file"));
        InputFileBoxBatch->setUrl(QUrl());
    }
}

void modCalcGalCoord::processLines(QTextStream &istream)
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
    SkyPoint sp;
    dms raB, decB, galLatB, galLongB;
    QString epoch0B;

    while (!istream.atEnd())
    {
        line = istream.readLine();
        line = line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split(' ');

        i = 0;

        // Input coords are galactic coordinates:

        if (galInputCoords)
        {
            // Read Galactic Longitude and write in ostream if corresponds

            if (galLongCheckBatch->isChecked())
            {
                galLongB = dms::fromString(fields[i], true);
                i++;
            }
            else
                galLongB = galLongBoxBatch->createDms(true);

            if (allRadioBatch->isChecked())
                ostream << galLongB.toDMSString() << space;
            else if (galLongCheckBatch->isChecked())
                ostream << galLongB.toDMSString() << space;

            // Read Galactic Latitude and write in ostream if corresponds

            if (galLatCheckBatch->isChecked())
            {
                galLatB = dms::fromString(fields[i], true);
                i++;
            }
            else
                galLatB = galLatBoxBatch->createDms(true);

            if (allRadioBatch->isChecked())
                ostream << galLatB.toDMSString() << space;
            else if (galLatCheckBatch->isChecked())
                ostream << galLatB.toDMSString() << space;

            sp = SkyPoint();
            sp.GalacticToEquatorial1950(&galLongB, &galLatB);
            ostream << sp.ra().toHMSString() << space << sp.dec().toDMSString() << epoch0B << '\n';
            // Input coords. are equatorial coordinates:
        }
        else
        {
            // Read RA and write in ostream if corresponds

            if (raCheckBatch->isChecked())
            {
                raB = dms::fromString(fields[i], false);
                i++;
            }
            else
                raB = raBoxBatch->createDms(false);

            if (allRadioBatch->isChecked())
                ostream << raB.toHMSString() << space;
            else if (raCheckBatch->isChecked())
                ostream << raB.toHMSString() << space;

            // Read DEC and write in ostream if corresponds

            if (decCheckBatch->isChecked())
            {
                decB = dms::fromString(fields[i], true);
                i++;
            }
            else
                decB = decBoxBatch->createDms();

            if (allRadioBatch->isChecked())
                ostream << decB.toDMSString() << space;
            else if (decCheckBatch->isChecked())
                ostream << decB.toDMSString() << space;

            // Read Epoch and write in ostream if corresponds

            if (epochCheckBatch->isChecked())
            {
                epoch0B = fields[i];
                i++;
            }
            else
                epoch0B = epochBoxBatch->text();

            if (allRadioBatch->isChecked())
                ostream << epoch0B << space;
            else if (epochCheckBatch->isChecked())
                ostream << epoch0B << space;

            sp = SkyPoint(raB, decB);
            sp.J2000ToB1950();
            sp.Equatorial1950ToGalactic(galLongB, galLatB);
            ostream << galLongB.toDMSString() << space << galLatB.toDMSString() << '\n';
        }
    }

    fOut.close();
}
