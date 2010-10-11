/***************************************************************************
                          lcgenerator.cpp  -  description
                             -------------------
    begin                : Tue Oct  1 18:01:48 CDT 2002
    copyright            : (C) 2002 by Jasem Mutlaq
    email                : mutlaqja@ku.edu
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "lcgenerator.h"

#include <QFile>
#include <QVBoxLayout>

#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "imageviewer.h"
#include "kstarsdata.h"

LCGeneratorUI::LCGeneratorUI( QWidget *p ) : QFrame(p) {
    setupUi( this );
}

LCGenerator::LCGenerator( QWidget* parent) :
    KDialog( parent )
{
    KStarsData* data = KStarsData::Instance();

    lcg = new LCGeneratorUI( this );
    setMainWidget( lcg );
    setCaption( i18n( "AAVSO Light Curve Generator" ) );
    setButtons( KDialog::Close );

    lcg->AverageDayBox->setMinimum( 1 );
    lcg->AverageDayBox->setValue( 1 );
    lcg->AverageDayBox->setSuffix(ki18np(" day", " days"));

    setModal( false );
    setWindowTitle(i18n( "AAVSO Light Curve Generator" ));
    downloadJob = 0;
    file = new QFile();

    lcg->DesignationBox->clear();
    lcg->NameBox->clear();

    lcg->StartDateBox->setDate(data->lt().date());
    lcg->EndDateBox->setDate(data->lt().date());

    readVarData();
    // Fill stars designations & names
    for (int i=0; i< (varInfoList.count()); i++) {
        lcg->DesignationBox->addItem( varInfoList.at(i).designation );
        lcg->NameBox->addItem( varInfoList.at(i).name );
    }


    // Signals/Slots
    QObject::connect(lcg->GetCurveButton, SIGNAL(clicked()), this,
                     SLOT(VerifyData()));
    QObject::connect(lcg->UpdateListButton, SIGNAL(clicked()), this,
                     SLOT(updateStarList()));
    QObject::connect(lcg->DesignationBox, SIGNAL(currentRowChanged(int)), this,
                     SLOT(updateNameList(int)));
    QObject::connect(lcg->NameBox, SIGNAL(currentRowChanged(int)), this,
                     SLOT(updateDesigList(int)));
}

LCGenerator::~LCGenerator()
{
    delete file;
    delete downloadJob;
}

void LCGenerator::VerifyData()
{
    QListWidgetItem* item = lcg->DesignationBox->currentItem();
    if( !item )
        return;
    if ( ! lcg->StartDateBox->date().isValid() ) {
        KMessageBox::error(this, i18n("Start date invalid."));
        return;
    }
    if ( ! lcg->EndDateBox->date().isValid() ) {
        KMessageBox::error(this, i18n("End date invalid."));
        return;
    }

    QDate StartDate    = lcg->StartDateBox->date();
    QDate EndDate      = lcg->EndDateBox->date();
    if (EndDate < StartDate) {
        KMessageBox::error(this, i18n("End date must occur after start date."));
        return;
    }

    //Download the curve!
    DownloadCurve(StartDate, EndDate, item->text(), lcg->AverageDayBox->value());
}

void LCGenerator::DownloadCurve(const QDate &StartDate, const QDate &EndDate, const QString &Designation, unsigned int AverageDay)
{
    QString buf = "http://www.aavso.org/cgi-bin/kstar.pl";
    QString Yes = "yes";
    QString No  = "no";

    buf.append('?'+QString::number(StartDate.toJulianDay()));
    buf.append('?'+QString::number(EndDate.toJulianDay()));
    buf.append('?'+Designation);
    buf.append('?'+QString::number(AverageDay));
    buf.append('?'+ (lcg->FainterCheck->isChecked() ? Yes : No));
    buf.append('?'+ (lcg->CCDVCheck->isChecked() ? Yes : No));
    buf.append('?'+ (lcg->CCDICheck->isChecked() ? Yes : No));
    buf.append('?'+ (lcg->CCDRCheck->isChecked() ? Yes : No));
    buf.append('?'+ (lcg->CCDBCheck->isChecked() ? Yes : No));
    buf.append('?'+ (lcg->VisualCheck->isChecked() ? Yes : No));
    buf.append('?'+ (lcg->DiscrepantCheck->isChecked() ? Yes : No));

    KUrl url(buf);
    QString message = i18n( "Light Curve produced by the American Amateur Variable Star Observers" );

    ImageViewer *iv = new ImageViewer( url, message, this );
    iv->show();
}

void LCGenerator::updateDesigList(int index)
{
    lcg->DesignationBox->setCurrentRow(index);
    //lcg->DesignationBox->centerCurrentItem();
}

void LCGenerator::updateNameList(int index)
{
    lcg->NameBox->setCurrentRow(index);
    //lcg->NameBox->centerCurrentItem();
}

void LCGenerator::updateStarList()
{
    file->setFileName( KStandardDirs::locateLocal( "appdata", "valaav.txt" ) );

    KUrl AAVSOFile("http://www.aavso.org/observing/aids/valaav.txt");
    KUrl saveFile (file->fileName());

    downloadJob = KIO::file_copy(AAVSOFile, saveFile, -1, KIO::Overwrite);
    connect (downloadJob, SIGNAL (result (KJob *)), SLOT (downloadReady (KJob *)));
}

void LCGenerator::downloadReady(KJob * job)
{
    downloadJob = 0;
    if ( job->error() )
    {
        KMessageBox::error(0, job->errorString());
        return;
    }

    file->close(); // to get the newest information of the file and not any information from opening of the file

    if ( file->exists() )
    {
        readVarData();

        lcg->DesignationBox->clear();
        lcg->NameBox->clear();

        // Fill stars designations
        for (int i=0; i < varInfoList.count(); i++) {
            lcg->DesignationBox->addItem( varInfoList.at(i).designation );
            lcg->NameBox->addItem( varInfoList.at(i).name );
        }

        KMessageBox::information(this, i18n("AAVSO Star list downloaded successfully."));
        return;
    }
}

bool LCGenerator::readVarData()
{
    QString varFile   = "valaav.txt";
    QString localName =  KStandardDirs::locateLocal( "appdata", varFile );
    QFile file;

    file.setFileName( localName );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        // Copy file with varstars data to user's data dir
        QFile::copy(KStandardDirs::locate("appdata", varFile), localName);
        if( !file.open( QIODevice::ReadOnly ) )
            return false;
    }

    QTextStream stream(&file);
    stream.readLine();

    VariableStarInfo vInfo;
    while(!stream.atEnd()) {
        QString line = stream.readLine();
        if( line.startsWith('*') )
            break;

        vInfo.designation = line.left(8).trimmed();
        vInfo.name        = line.mid(10,20).simplified();
        varInfoList.append(vInfo);
    }
    return true;
}

void LCGenerator::closeEvent (QCloseEvent *ev)
{
    if (ev)	// not if closeEvent (0) is called, because segfault
        ev->accept();	// parent-widgets should not get this event, so it will be filtered
    //this->~LCGenerator();	// destroy the object, so the object can only allocated with operator new, not as a global/local variable
}

#include "lcgenerator.moc"
