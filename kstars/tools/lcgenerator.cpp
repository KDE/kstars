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

#include <QFile>
#include <QVBoxLayout>

#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kapplication.h>

#include "lcgenerator.h"
#include "imageviewer.h"
#include "kstars.h"
#include "kstarsdata.h"

LCGeneratorUI::LCGeneratorUI( QWidget *p ) : QFrame(p) {
	setupUi( this );
}

LCGenerator::LCGenerator( QWidget* parent)
	: KDialog( parent ),
		Hostprefix("http://www.aavso.org/cgi-bin/kstar.pl"), JDCutOff(2437600)
{
	ksw = (KStars*) parent;
	lcg = new LCGeneratorUI( this );
	setMainWidget( lcg );
        setCaption( i18n( "AAVSO Light Curve Generator" ) );
        setButtons( KDialog::Close );

        lcg->AverageDayBox->setMinimum( 1 );
	lcg->AverageDayBox->setValue( 1 );

	setWindowTitle(i18n( "AAVSO Light Curve Generator" ));
	downloadJob = 0;
	file = new QFile();

	lcg->DesignationBox->clear();
        lcg->NameBox->clear();

	// FIXME ExDateEdit is broken, check LCGenerator again
	// When it gets fixed
	lcg->StartDateBox->setRange(-20000000, 20000000);
	lcg->EndDateBox->setRange(-20000000, 20000000);
	lcg->StartDateBox->setDate(ksw->data()->lt().date());
	lcg->EndDateBox->setDate(ksw->data()->lt().date());

	// Fill stars designations
         for (int i=0; i< (ksw->data()->VariableStarsList.count()); i++)
                lcg->DesignationBox->addItem(ksw->data()->VariableStarsList.at(i)->Designation);

         // Fill star names
         for (int i=0; i<ksw->data()->VariableStarsList.count(); i++)
                lcg->NameBox->addItem(ksw->data()->VariableStarsList.at(i)->Name);


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
	ExtDate StartDate, EndDate;
	QString Designation, AverageDays;

	if ( ! lcg->StartDateBox->date().isValid() ) {
	  KMessageBox::error(this, i18n("Start date inavlid."));
	  return;
	}
	if ( ! lcg->EndDateBox->date().isValid() ) {
	  KMessageBox::error(this, i18n("End date inavlid."));
	  return;
	}

	StartDate    = lcg->StartDateBox->date();
	EndDate      = lcg->EndDateBox->date();
	if (EndDate < StartDate) {
	  KMessageBox::error(this, i18n("End date must occur after start date."));
	  return;
	}

	// Check that we have an integer for average number of days, if data field empty, then make it 'default'
	AverageDays  = lcg->AverageDayBox->value();
	Designation  = lcg->DesignationBox->currentItem()->text();

	//Download the curve!
	DownloadCurve(StartDate, EndDate, Designation, AverageDays);

}

void LCGenerator::DownloadCurve(const ExtDate &StartDate, const ExtDate &EndDate, const QString &Designation, const QString &AverageDay)
{

	QString buf(Hostprefix);
	QString Yes("yes");
	QString No("no");

	buf.append('?'+QString::number(StartDate.jd()));
	buf.append('?'+QString::number(EndDate.jd()));
	buf.append('?'+Designation);
	buf.append('?'+AverageDay);
	buf.append('?'+ (lcg->FainterCheck->isChecked() ? Yes : No));
	buf.append('?'+ (lcg->CCDVCheck->isChecked() ? Yes : No));
	buf.append('?'+ (lcg->CCDICheck->isChecked() ? Yes : No));
	buf.append('?'+ (lcg->CCDRCheck->isChecked() ? Yes : No));
	buf.append('?'+ (lcg->CCDBCheck->isChecked() ? Yes : No));
	buf.append('?'+ (lcg->VisualCheck->isChecked() ? Yes : No));
	buf.append('?'+ (lcg->DiscrepantCheck->isChecked() ? Yes : No));


	KUrl url(buf);
	QString message = i18n( "Light Curve produced by the American Amateur Variable Star Observers" );
	// parent of imageview is KStars
	new ImageViewer(url, message, ksw);

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

	downloadJob = KIO::file_copy (AAVSOFile, saveFile, -1, true);
	connect (downloadJob, SIGNAL (result (KJob *)), SLOT (downloadReady (KJob *)));
}

void LCGenerator::downloadReady(KJob * job)
{

downloadJob = 0;

	if ( job->error() )
	{
		static_cast<KIO::Job*>(job)->showErrorDialog();
		closeEvent (0);
		return;		// exit this function
	}

	file->close(); // to get the newest information of the file and not any information from opening of the file

	if ( file->exists() )
	{
		ksw->data()->readVARData();

		lcg->DesignationBox->clear();
                lcg->NameBox->clear();

		// Fill stars designations
                for (int i=0; i< (ksw->data()->VariableStarsList.count()); i++)
                lcg->DesignationBox->addItem(ksw->data()->VariableStarsList.at(i)->Designation);

                // Fill star names
                for (int i=0; i<ksw->data()->VariableStarsList.count(); i++)
                lcg->NameBox->addItem(ksw->data()->VariableStarsList.at(i)->Name);

                KMessageBox::information(this, i18n("AAVSO Star list downloaded successfully."));


		return;
	}
	closeEvent (0);

}

void LCGenerator::closeEvent (QCloseEvent *ev)
{
	if (ev)	// not if closeEvent (0) is called, because segfault
		ev->accept();	// parent-widgets should not get this event, so it will be filtered
	//this->~LCGenerator();	// destroy the object, so the object can only allocated with operator new, not as a global/local variable
}

#include "lcgenerator.moc"
