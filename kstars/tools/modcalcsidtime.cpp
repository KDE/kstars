/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcsidtime.h"

#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksnotification.h"
#include "dialogs/locationdialog.h"

#include <KLineEdit>

#include <QTextStream>

modCalcSidTime::modCalcSidTime(QWidget *parent) : QFrame(parent)
{
    setupUi(this);

    //Preset date and location
    showCurrentTimeAndLocation();

    // signals and slots connections
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotChangeLocation()));
    connect(Date, SIGNAL(dateChanged(QDate)), this, SLOT(slotChangeDate()));
    connect(LT, SIGNAL(timeChanged(QTime)), this, SLOT(slotConvertST(QTime)));
    connect(ST, SIGNAL(timeChanged(QTime)), this, SLOT(slotConvertLT(QTime)));

    connect(LocationCheckBatch, SIGNAL(clicked()), this, SLOT(slotLocationChecked()));
    connect(DateCheckBatch, SIGNAL(clicked()), this, SLOT(slotDateChecked()));
    connect(LocationCheckBatch, SIGNAL(clicked()), this, SLOT(slotHelpLabel()));
    connect(DateCheckBatch, SIGNAL(clicked()), this, SLOT(slotHelpLabel()));
    connect(ComputeComboBatch, SIGNAL(currentIndexChanged(int)), this, SLOT(slotHelpLabel()));

    connect(InputFileBatch, SIGNAL(urlSelected(QUrl)), this, SLOT(slotCheckFiles()));
    connect(OutputFileBatch, SIGNAL(urlSelected(QUrl)), this, SLOT(slotCheckFiles()));
    connect(LocationButtonBatch, SIGNAL(clicked()), this, SLOT(slotLocationBatch()));
    connect(RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));
    connect(ViewButtonBatch, SIGNAL(clicked()), this, SLOT(slotViewBatch()));

    RunButtonBatch->setEnabled(false);
    ViewButtonBatch->setEnabled(false);

    show();
}

void modCalcSidTime::showCurrentTimeAndLocation()
{
    KStarsData *data = KStarsData::Instance();
    LT->setTime(data->lt().time());
    Date->setDate(data->lt().date());

    geo = data->geo();
    LocationButton->setText(geo->fullName());
    geoBatch = data->geo();
    LocationButtonBatch->setText(geoBatch->fullName());

    slotConvertST(LT->time());
}

void modCalcSidTime::slotChangeLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);

    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geo = newGeo;
            LocationButton->setText(geo->fullName());

            //Update the displayed ST
            slotConvertST(LT->time());
        }
    }
    delete ld;
}

void modCalcSidTime::slotChangeDate()
{
    slotConvertST(LT->time());
}

void modCalcSidTime::slotConvertST(const QTime &lt)
{
    // blockSignals is used to break signal loop
    ST->blockSignals(true);
    ST->setTime(computeLTtoST(lt));
    ST->blockSignals(false);
}

void modCalcSidTime::slotConvertLT(const QTime &st)
{
    // blockSignals is used to break signal loop
    LT->blockSignals(true);
    LT->setTime(computeSTtoLT(st));
    LT->blockSignals(false);
}

QTime modCalcSidTime::computeLTtoST(QTime lt)
{
    KStarsDateTime utdt = geo->LTtoUT(KStarsDateTime(Date->date(), lt));
    dms st              = geo->GSTtoLST(utdt.gst());
    return QTime(st.hour(), st.minute(), st.second());
}

QTime modCalcSidTime::computeSTtoLT(QTime st)
{
    KStarsDateTime dt0 = KStarsDateTime(Date->date(), QTime(0, 0, 0));
    dms lst;
    lst.setH(st.hour(), st.minute(), st.second());
    dms gst = geo->LSTtoGST(lst);
    return geo->UTtoLT(KStarsDateTime(Date->date(), dt0.GSTtoUT(gst))).time();
}

//** Batch mode **//
void modCalcSidTime::slotDateChecked()
{
    DateBatch->setEnabled(!DateCheckBatch->isChecked());
}

void modCalcSidTime::slotLocationChecked()
{
    LocationButtonBatch->setEnabled(!LocationCheckBatch->isChecked());

    if (LocationCheckBatch->isChecked())
    {
        QString message = i18n("Location strings consist of the "
                               "comma-separated names of the city, province and country.  "
                               "If the string contains spaces, enclose it in quotes so it "
                               "gets parsed properly.");

        KMessageBox::information(nullptr, message, i18n("Hint for writing location strings"),
                                 "DontShowLocationStringMessageBox");
    }
}

void modCalcSidTime::slotHelpLabel()
{
    QStringList inList;
    if (ComputeComboBatch->currentIndex() == 0)
        inList.append(i18n("local time"));
    else
        inList.append(i18n("sidereal time"));

    if (DateCheckBatch->checkState() == Qt::Checked)
        inList.append(i18n("date"));

    if (LocationCheckBatch->checkState() == Qt::Checked)
        inList.append(i18n("location"));

    QString inListString = inList[0];
    if (inList.size() == 2)
        inListString = i18n("%1 and %2", inList[0], inList[1]);
    if (inList.size() == 3)
        inListString = i18n("%1, %2 and %3", inList[0], inList[1], inList[2]);

    HelpLabel->setText(i18n("Specify %1 in the input file.", inListString));
}

void modCalcSidTime::slotLocationBatch()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);

    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geoBatch = newGeo;
            LocationButtonBatch->setText(geoBatch->fullName());
        }
    }
    delete ld;
}

void modCalcSidTime::slotCheckFiles()
{
    if (!InputFileBatch->lineEdit()->text().isEmpty() && !OutputFileBatch->lineEdit()->text().isEmpty())
    {
        RunButtonBatch->setEnabled(true);
    }
    else
    {
        RunButtonBatch->setEnabled(false);
    }
}

void modCalcSidTime::slotRunBatch()
{
    QString inputFileName = InputFileBatch->url().toLocalFile();

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

        QTextStream istream(&f);
        processLines(istream);

        ViewButtonBatch->setEnabled(true);

        f.close();
    }
    else
    {
        QString message = i18n("Invalid file: %1", inputFileName);
        KSNotification::sorry(message, i18n("Invalid file"));
        inputFileName.clear();
        return;
    }
}

void modCalcSidTime::processLines(QTextStream &istream)
{
    QFile fOut(OutputFileBatch->url().toLocalFile());
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    QString line;
    dms LST;
    QTime inTime, outTime;
    QDate dt;

    if (!DateCheckBatch->isChecked())
        dt = DateBatch->date();

    while (!istream.atEnd())
    {
        line = istream.readLine();
        line = line.trimmed();

        QStringList fields = line.split(' ', QString::SkipEmptyParts);

        //Find and parse the location string
        if (LocationCheckBatch->isChecked())
        {
            //First, look for a pair of quotation marks, and parse the string between them
            QChar q = '\"';
            if (line.indexOf(q) == -1)
                q = '\'';
            if (line.count(q) == 2)
            {
                int iStart             = line.indexOf(q);
                int iEnd               = line.indexOf(q, iStart + 1);
                QString locationString = line.mid(iStart, iEnd - iStart + 1);
                line.remove(locationString);
                fields = line.split(' ', QString::SkipEmptyParts);
                locationString.remove(q);

                QStringList locationFields = locationString.split(',', QString::SkipEmptyParts);
                for (int i = 0; i < locationFields.size(); i++)
                    locationFields[i] = locationFields[i].trimmed();

                if (locationFields.size() == 1)
                    locationFields.insert(1, "");
                if (locationFields.size() == 2)
                    locationFields.insert(1, "");
                if (locationFields.size() != 3)
                {
                    qDebug() << "Error: could not parse location string: " << locationString;
                    continue;
                }

                geoBatch = KStarsData::Instance()->locationNamed(locationFields[0], locationFields[1],
                           locationFields[2]);
                if (geoBatch == nullptr)
                {
                    qDebug() << "Error: location not found in database: " << locationString;
                    continue;
                }
            }
        }

        if (DateCheckBatch->isChecked())
        {
            //Parse one of the fields as the date
            for (auto &s : fields)
            {
                dt = QDate::fromString(s);
                if (dt.isValid())
                    break;
            }
            if (!dt.isValid())
            {
                qDebug() << "Error: did not find a valid date string in: " << line;
                continue;
            }
        }

        //Parse one of the fields as the time
        for (auto &s : fields)
        {
            if (s.contains(':'))
            {
                inTime = QTime::fromString(s.length() == 4 ? '0' + s : s);
                if (inTime.isValid())
                    break;
            }
        }
        if (!inTime.isValid())
        {
            qDebug() << "Error: did not find a valid time string in: " << line;
            continue;
        }

        if (geoBatch != nullptr)
        {
            if (ComputeComboBatch->currentIndex() == 0)
            {
                //inTime is the local time, compute LST
                KStarsDateTime ksdt(dt, inTime);
                ksdt    = geoBatch->LTtoUT(ksdt);
                dms lst = geoBatch->GSTtoLST(ksdt.gst());
                outTime = QTime(lst.hour(), lst.minute(), lst.second());
            }
            else
            {
                //inTime is the sidereal time, compute the local time
                KStarsDateTime ksdt(dt, QTime(0, 0, 0));
                dms lst;
                lst.setH(inTime.hour(), inTime.minute(), inTime.second());
                QTime ut = ksdt.GSTtoUT(geoBatch->LSTtoGST(lst));
                ksdt.setTime(ut);
                ksdt    = geoBatch->UTtoLT(ksdt);
                outTime = ksdt.time();
            }

            //Write to output file
            ostream << QLocale().toString(dt, QLocale::LongFormat) << "  \"" << geoBatch->fullName() << "\"  "
                    << QLocale().toString(inTime) << "  " << QLocale().toString(outTime) << '\n';
        }
    }

    fOut.close();
}

void modCalcSidTime::slotViewBatch()
{
    QFile fOut(OutputFileBatch->url().toLocalFile());
    fOut.open(QIODevice::ReadOnly);
    QTextStream istream(&fOut);
    QStringList text;

    while (!istream.atEnd())
        text.append(istream.readLine());

    fOut.close();

    KMessageBox::informationList(nullptr, i18n("Results of Sidereal time calculation"), text,
                                 OutputFileBatch->url().toLocalFile());
}
