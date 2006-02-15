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

LCGeneratorUI::LCGeneratorUI( QWidget *p ) {
	setupUi( p );
}

LCGenerator::LCGenerator( QWidget* parent)
	: KDialogBase( KDialogBase::Plain, i18n( "AAVSO Light Curve Generator" ), 
				Close, Close, parent ),
	 Hostprefix("http://www.aavso.org/cgi-bin/kstar.pl"), JDCutOff(2437600)
{
	QFrame *page = plainPage();
	ksw = (KStars*) parent;
	vlay = new QVBoxLayout( page, 0, spacingHint() );
	lcg = new LCGeneratorUI( page );
	lcg->AverageDayBox->setMinimum( 1 );
	lcg->AverageDayBox->setValue( 1 );
	vlay->addWidget( lcg );

	downloadJob = 0;
	file = new QFile();

	// Signals/Slots
	QObject::connect(lcg->GetCurveButton, SIGNAL(clicked()), this,
			SLOT(VerifyData()));
	QObject::connect(lcg->UpdateListButton, SIGNAL(clicked()), this, 
			SLOT(updateStarList()));
	QObject::connect(lcg->DesignationBox, SIGNAL(highlighted(int)), this, 
			SLOT(updateNameList(int)));
	QObject::connect(lcg->NameBox, SIGNAL(highlighted(int)), this, 
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
	Designation  = lcg->DesignationBox->currentText();

	//Download the curve!
	DownloadCurve(StartDate, EndDate, Designation, AverageDays);
  
}

void LCGenerator::DownloadCurve(const ExtDate &StartDate, const ExtDate &EndDate, const QString &Designation, const QString &AverageDay)
{

	QString buf(Hostprefix);
	QString Yes("yes");
	QString No("no");
	
	buf.append("?"+QString::number(StartDate.jd()));
	buf.append("?"+QString::number(EndDate.jd()));
	buf.append("?"+Designation);
	buf.append("?"+AverageDay);
	buf.append("?"+ (lcg->FainterCheck->isOn() ? Yes : No));
	buf.append("?"+ (lcg->CCDVCheck->isOn() ? Yes : No));
	buf.append("?"+ (lcg->CCDICheck->isOn() ? Yes : No));
	buf.append("?"+ (lcg->CCDRCheck->isOn() ? Yes : No));
	buf.append("?"+ (lcg->CCDBCheck->isOn() ? Yes : No));
	buf.append("?"+ (lcg->VisualCheck->isOn() ? Yes : No));
	buf.append("?"+ (lcg->DiscrepantCheck->isOn() ? Yes : No));
	

	KUrl url(buf);
	QString message = i18n( "Light Curve produced by the American Amateur Variable Star Observers" );
	// parent of imageview is KStars
	new ImageViewer(&url, message, ksw, "lightcurve");
        
}

void LCGenerator::updateDesigList(int index)
{

    lcg->DesignationBox->setSelected(index, true);
    lcg->DesignationBox->centerCurrentItem();
    
}

void LCGenerator::updateNameList(int index)
{

    lcg->NameBox->setSelected(index, true);
    lcg->NameBox->centerCurrentItem();
    
}

void LCGenerator::updateStarList()
{
	file->setName( locateLocal( "appdata", "valaav.txt" ) );
	
	KUrl AAVSOFile("http://www.aavso.org/observing/aids/valaav.txt");
	KUrl saveFile (file->name());
	
	downloadJob = KIO::file_copy (AAVSOFile, saveFile, -1, true);
	connect (downloadJob, SIGNAL (result (KIO::Job *)), SLOT (downloadReady (KIO::Job *)));
}

void LCGenerator::downloadReady(KIO::Job * job)
{

downloadJob = 0;

	if ( job->error() )
	{
		job->showErrorDialog();
		closeEvent (0);
		return;		// exit this function
	}

	file->close(); // to get the newest informations of the file and not any informations from opening of the file

	if ( file->exists() )
	{
		ksw->data()->readVARData();
		
		lcg->DesignationBox->clear();
                lcg->NameBox->clear();
		
		// Fill stars designations
                for (uint i=0; i< (ksw->data()->VariableStarsList.count()); i++)
                lcg->DesignationBox->insertItem(ksw->data()->VariableStarsList.at(i)->Designation);

                // Fill star names
                for (uint i=0; i<ksw->data()->VariableStarsList.count(); i++)
                lcg->NameBox->insertItem(ksw->data()->VariableStarsList.at(i)->Name);
		
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
