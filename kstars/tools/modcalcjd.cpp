/***************************************************************************
                          modcalcjd.cpp  -  description
                             -------------------
    begin                : Tue Jan 15 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcjd.h"
#include "modcalcjd.moc"

#include <KGlobal>
#include <KLocale>
#include <qradiobutton.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <QDebug>

#include <QtCore/QTextStream>

#include "kstarsdatetime.h"

#define MJD0 2400000.5

modCalcJD::modCalcJD(QWidget *parentSplit) : QFrame(parentSplit) {
    setupUi(this);

    // signals and slots connections
    connect(NowButton, SIGNAL(clicked()), this, SLOT(showCurrentTime()));
    connect( DateTimeBox, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(slotUpdateCalendar()) );
    connect( JDBox, SIGNAL(editingFinished()), this, SLOT(slotUpdateJD()) );
    connect( ModJDBox, SIGNAL(editingFinished()), this, SLOT(slotUpdateModJD()) );
    connect( InputFileBatch, SIGNAL(urlSelected(const QUrl&)), this, SLOT(slotCheckFiles()) );
    connect( OutputFileBatch, SIGNAL(urlSelected(const QUrl&)), this, SLOT(slotCheckFiles()) );
    connect( RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()) );
    connect( ViewButtonBatch, SIGNAL(clicked()), this, SLOT(slotViewBatch()) );

    RunButtonBatch->setEnabled( false );
    ViewButtonBatch->setEnabled( false );

    showCurrentTime();
    slotUpdateCalendar();
    show();
}

modCalcJD::~modCalcJD(void)
{
}

void modCalcJD::slotUpdateCalendar()
{
    long double julianDay, modjulianDay;

    julianDay = KStarsDateTime(DateTimeBox->dateTime()).djd();
    showJd( julianDay );

    modjulianDay = julianDay - MJD0;
    showMjd(modjulianDay);
}

void modCalcJD::slotUpdateModJD()
{
    long double julianDay, modjulianDay;

    modjulianDay = KLocale::global()->readNumber( ModJDBox->text() );
    julianDay = MJD0 + modjulianDay;
    showJd( julianDay );
    DateTimeBox->setDateTime( KStarsDateTime( julianDay ).dateTime() );
}

void modCalcJD::slotUpdateJD()
{
    long double julianDay, modjulianDay;
    julianDay = KLocale::global()->readNumber( JDBox->text() );
    KStarsDateTime dt( julianDay );

    DateTimeBox->setDateTime( dt.dateTime() );

    modjulianDay = julianDay - MJD0;
    showMjd( modjulianDay );
}


void modCalcJD::showCurrentTime (void)
{
    DateTimeBox->setDateTime( KStarsDateTime::currentDateTime().dateTime() );
}

void modCalcJD::showJd(long double julianDay)
{
    JDBox->setText(QLocale().toString( (double)julianDay, 5 ) );
}

void modCalcJD::showMjd(long double modjulianDay)
{
    ModJDBox->setText(QLocale().toString( (double)modjulianDay, 5 ) );
}

void modCalcJD::slotCheckFiles() {
    if ( ! InputFileBatch->lineEdit()->text().isEmpty() && ! OutputFileBatch->lineEdit()->text().isEmpty() ) {
        RunButtonBatch->setEnabled( true );
    } else {
        RunButtonBatch->setEnabled( false );
    }
}

void modCalcJD::slotRunBatch() {
    QString inputFileName = InputFileBatch->url().toLocalFile();

    if ( QFile::exists(inputFileName) ) {
        QFile f( inputFileName );
        if ( !f.open( QIODevice::ReadOnly) ) {
            QString message = xi18n( "Could not open file %1.", f.fileName() );
            KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
            return;
        }

        QTextStream istream(&f);
        processLines( istream, InputComboBatch->currentIndex() );
        ViewButtonBatch->setEnabled( true );

        f.close();
    } else  {
        QString message = xi18n( "Invalid file: %1", inputFileName );
        KMessageBox::sorry( 0, message, xi18n( "Invalid file" ) );
        return;
    }
}

void modCalcJD::processLines( QTextStream &istream, int inputData ) {
    QFile fOut( OutputFileBatch->url().toLocalFile() );
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    QString line;
    long double jd(0);
    double mjd(0);
    KStarsDateTime dt;

    while ( ! istream.atEnd() ) {
        line = istream.readLine();
        line = line.trimmed();
        QStringList data = line.split( ' ', QString::SkipEmptyParts );

        if ( inputData == 0 ) { //Parse date & time
            //Is the first field parseable as a date or date&time?
            if ( data[0].length() > 10 )
                dt = KStarsDateTime::fromString( data[0] );
            else
                dt = KStarsDateTime( QDate::fromString( data[0] ), QTime(0,0,0) );

            //DEBUG
            qDebug() << data[0];
            if ( dt.isValid() ) qDebug() << dt.toString();

            if ( dt.isValid() ) {
                //Try to parse the second field as a time
                if ( data.size() > 1 ) {
                    QString s = data[1];
                    if ( s.length() == 4 ) s = '0'+s;
                    QTime t = QTime::fromString( s );
                    if ( t.isValid() ) dt.setTime( t );
                }

            } else { //Did not parse the first field as a date; try it as a time
                QTime t = QTime::fromString( data[0] );
                if ( t.isValid() ) {
                    dt.setTime( t );
                    //Now try the second field as a date
                    if ( data.size() > 1 ) {
                        QDate d = QDate::fromString( data[1] );
                        if ( d.isValid() ) dt.setDate( d );
                        else dt.setDate( QDate::currentDate() );
                    }
                }
            }

            if ( dt.isValid() ) {
                //Compute JD and MJD
                jd = dt.djd();
                mjd = jd - MJD0;
            }

        } else if ( inputData == 1 ) {//Parse Julian day
            bool ok(false);
            jd = data[0].toDouble(&ok);
            if ( ok ) {
                dt.setDJD( jd );
                mjd = jd - MJD0;
            }
        } else if ( inputData == 2 ) {//Parse Modified Julian day
            bool ok(false);
            mjd = data[0].toDouble(&ok);
            if ( ok ) {
                jd = mjd + MJD0;
                dt.setDJD( jd );
            }
        }

        //Write to output file
        ostream << QLocale().toStringTime( dt, KLocale::LongDate ) << "  "
        << QString::number( jd, 'f', 2 ) << "  "
        << QString::number( mjd, 'f', 2 ) << endl;

    }

    fOut.close();
}

void modCalcJD::slotViewBatch() {
    QFile fOut( OutputFileBatch->url().toLocalFile() );
    fOut.open(QIODevice::ReadOnly);
    QTextStream istream(&fOut);
    QStringList text;

    while ( ! istream.atEnd() )
        text.append( istream.readLine() );

    fOut.close();

    KMessageBox::informationList( 0, xi18n("Results of Julian day calculation"), text, OutputFileBatch->url().toLocalFile() );
}
