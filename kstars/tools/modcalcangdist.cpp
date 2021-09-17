/*
    SPDX-FileCopyrightText: 2004 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcangdist.h"

#include "dms.h"
#include "kstars.h"
#include "ksnotification.h"
#include "dialogs/finddialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/skypoint.h"
#include "widgets/dmsbox.h"

#include <KLocalizedString>

#include <QTextStream>
#include <QPointer>

modCalcAngDist::modCalcAngDist(QWidget *parentSplit) : QFrame(parentSplit)
{
    setupUi(this);
    FirstRA->setDegType(false);
    SecondRA->setDegType(false);

    connect(FirstRA, SIGNAL(editingFinished()), this, SLOT(slotValidatePositions()));
    connect(FirstDec, SIGNAL(editingFinished()), this, SLOT(slotValidatePositions()));
    connect(SecondRA, SIGNAL(editingFinished()), this, SLOT(slotValidatePositions()));
    connect(SecondDec, SIGNAL(editingFinished()), this, SLOT(slotValidatePositions()));
    connect(FirstRA, SIGNAL(textEdited(QString)), this, SLOT(slotResetTitle()));
    connect(FirstDec, SIGNAL(textEdited(QString)), this, SLOT(slotResetTitle()));
    connect(SecondRA, SIGNAL(textEdited(QString)), this, SLOT(slotResetTitle()));
    connect(SecondDec, SIGNAL(textEdited(QString)), this, SLOT(slotResetTitle()));
    connect(FirstObjectButton, SIGNAL(clicked()), this, SLOT(slotObjectButton()));
    connect(SecondObjectButton, SIGNAL(clicked()), this, SLOT(slotObjectButton()));
    connect(runButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));

    show();
    slotValidatePositions();
}

SkyPoint modCalcAngDist::getCoords(dmsBox *rBox, dmsBox *dBox, bool *ok)
{
    dms raCoord, decCoord;

    bool ok2 = false;
    raCoord  = rBox->createDms(false, &ok2);
    if (ok2)
        decCoord = dBox->createDms(true, &ok2);

    if (ok2)
    {
        if (ok)
            *ok = ok2;
        return SkyPoint(raCoord, decCoord);
    }
    else
    {
        if (ok)
            *ok = ok2;
        return SkyPoint();
    }
}

void modCalcAngDist::slotValidatePositions()
{
    SkyPoint sp0, sp1;
    bool ok;
    sp0 = getCoords(FirstRA, FirstDec, &ok);

    if (ok)
        sp1 = getCoords(SecondRA, SecondDec, &ok);

    if (ok)
    {
        double PA = 0;
        AngDist->setText(sp0.angularDistanceTo(&sp1, &PA).toDMSString());
        PositionAngle->setText(QString::number(PA, 'f', 3));
    }
    else
    {
        AngDist->setText(" .... ");
        PositionAngle->setText(" .... ");
    }
}

void modCalcAngDist::slotObjectButton()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *o = FindDialog::Instance()->targetObject();
        if (sender()->objectName() == QString("FirstObjectButton"))
        {
            FirstRA->showInHours(o->ra());
            FirstDec->showInDegrees(o->dec());
            FirstPositionBox->setTitle(i18n("First position: %1", o->name()));
        }
        else
        {
            SecondRA->showInHours(o->ra());
            SecondDec->showInDegrees(o->dec());
            SecondPositionBox->setTitle(i18n("Second position: %1", o->name()));
        }

        slotValidatePositions();
    }
}

void modCalcAngDist::slotResetTitle()
{
    QString name = sender()->objectName();
    if (name.contains("First"))
        FirstPositionBox->setTitle(i18n("First position"));
    else
        SecondPositionBox->setTitle(i18n("Second position"));
}

void modCalcAngDist::slotRunBatch()
{
    QString inputFileName = InputLineEditBatch->url().toLocalFile();

    // We open the input file and read its content

    if (QFile::exists(inputFileName))
    {
        QFile f(inputFileName);
        if (!f.open(QIODevice::ReadOnly))
        {
            QString message = i18n("Could not open file %1.", f.fileName());
            KSNotification::sorry(message, i18n("Could Not Open File"));
            inputFileName.clear();
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
        inputFileName.clear();
        InputLineEditBatch->setText(inputFileName);
        return;
    }
}

//void modCalcAngDist::processLines( const QFile * fIn ) {
void modCalcAngDist::processLines(QTextStream &istream)
{
    // we open the output file

    //	QTextStream istream(&fIn);
    QString outputFileName;
    outputFileName = OutputLineEditBatch->text();
    QFile fOut(outputFileName);
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    QString line;
    QChar space = ' ';
    int i       = 0;
    SkyPoint sp0, sp1;
    double PA = 0;
    dms ra0B, dec0B, ra1B, dec1B, dist;

    while (!istream.atEnd())
    {
        line = istream.readLine();
        line = line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split(' ');

        i = 0;

        // Read RA and write in ostream if corresponds

        if (ra0CheckBatch->isChecked())
        {
            ra0B = dms::fromString(fields[i], false);
            i++;
        }
        else
            ra0B = ra0BoxBatch->createDms(false);

        if (allRadioBatch->isChecked())
            ostream << ra0B.toHMSString() << space;
        else if (ra0CheckBatch->isChecked())
            ostream << ra0B.toHMSString() << space;

        // Read DEC and write in ostream if corresponds

        if (dec0CheckBatch->isChecked())
        {
            dec0B = dms::fromString(fields[i], true);
            i++;
        }
        else
            dec0B = dec0BoxBatch->createDms();

        if (allRadioBatch->isChecked())
            ostream << dec0B.toDMSString() << space;
        else if (dec0CheckBatch->isChecked())
            ostream << dec0B.toDMSString() << space;

        // Read RA and write in ostream if corresponds

        if (ra1CheckBatch->isChecked())
        {
            ra1B = dms::fromString(fields[i], false);
            i++;
        }
        else
            ra1B = ra1BoxBatch->createDms(false);

        if (allRadioBatch->isChecked())
            ostream << ra1B.toHMSString() << space;
        else if (ra1CheckBatch->isChecked())
            ostream << ra1B.toHMSString() << space;

        // Read DEC and write in ostream if corresponds

        if (dec1CheckBatch->isChecked())
        {
            dec1B = dms::fromString(fields[i], true);
            i++;
        }
        else
            dec1B = dec1BoxBatch->createDms();

        if (allRadioBatch->isChecked())
            ostream << dec1B.toDMSString() << space;
        else if (dec1CheckBatch->isChecked())
            ostream << dec1B.toDMSString() << space;

        sp0  = SkyPoint(ra0B, dec0B);
        sp1  = SkyPoint(ra1B, dec1B);
        dist = sp0.angularDistanceTo(&sp1, &PA);

        ostream << dist.toDMSString() << QString::number(PA, 'f', 3) << '\n';
    }

    fOut.close();
}
