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
	QString InitialStartDate, InitialEndDate;
	QString FinalDesignation, FinalStartDate, FinalEndDate, AverageDays;
	bool AverageDaysOK;

	// Get initial user input
	if ( lcg->StartDateBox->text().isEmpty() ) lcg->StartDateBox->setText( i18n( "default" ) );
	if ( lcg->EndDateBox->text().isEmpty() ) EndDateIn->setText( i18n( "default" ) );
    InitialStartDate     = StartDateIn->text().lower();
    InitialEndDate       = EndDateIn->text().lower();
    AverageDays       =  AverageDayIn->text();
    FinalDesignation = DesignationIn->currentText();

    // set Julian day
    if (!setJD(InitialStartDate, &FinalStartDate, 0))
        return;
    if (!setJD(InitialEndDate, &FinalEndDate, 1))
        return;

    if (FinalEndDate.toInt() < FinalStartDate.toInt())
    {
        KMessageBox::error(this, i18n("End date must occur after start date."));
        return;
    }

   // Check that we have an integer for average number of days, if data field empty, then make it 'default'
   if (!AverageDays.isEmpty())
   {
        AverageDays.toInt(&AverageDaysOK);
        if (!AverageDaysOK)
        {
            KMessageBox::error(this, i18n("Average days must be a positive integer."));
            return;
         }
	 else
	 {
	  if (AverageDays.toInt() < 0)
	  {
	  	KMessageBox::error(this, i18n("Average days must be a positive integer."));
            	return;
	  }
	 }
	  
   }
   else AverageDays = QString("default");

    //Download the curve!
   DownloadCurve(FinalStartDate, FinalEndDate, FinalDesignation, AverageDays);
  
}

bool LCGenerator::setJD(QString Date, QString *JD, int JDType)
{
    uint i=0;
    int TempJD=0;
    int slashCount =0;
    int slashRefrence[2];

    int dateFormat[3];
    bool isNumber;
    
    const QString invalidFormatStartJD(i18n("Invalid date format. Correct format is mm/dd/yyyy or JD, leave 'default' to generate light curves for the past 500 days."));
    const QString invalidFormatENDJD(i18n("Invalid date format. Correct format is mm/dd/yyyy or JD, leave 'default' to generate light curves until today."));
    QString invalidFormatMsg(JDType ? invalidFormatENDJD : invalidFormatStartJD);
    

    // check for "default" date
    if (Date == i18n("default"))
    {
       *JD = "default";
       return true;
    }

    // Get slashcount and and slash refrences
    for (i=0; i<Date.length(); i++)
      if (Date.at(i) == '/')
      {
          slashRefrence[slashCount++] = i;
          if (slashCount > 2)
          {
             KMessageBox::error(this, invalidFormatMsg);
             return false;
          }
      }

    // check if the data appears to be in JD format
    if (!slashCount)
    {
        TempJD = Date.toInt(&isNumber, 10);
        if (!isNumber)
        {
             KMessageBox::error(this, invalidFormatMsg);
             return false;
        }

         if (TempJD >= JDCutOff)
        {
          JD->setNum(TempJD, 10);
          return true;
        }
        else
        {
             const char* invalidJD = I18N_NOOP("No data available for JD prior to %d");
             KMessageBox::error(this, QString().sprintf(invalidJD, JDCutOff));
             return false;
        }
    }

    // If it's not a Julian day, check for the format of the date
    // check if year is 4 digits
    if ( (Date.length() - slashRefrence[1] - 1) != 4)
     {
          KMessageBox::error(this, invalidFormatMsg);
          return false;
     }

    // form mm/dd/yyy fields
    dateFormat[0] = Date.mid(0, slashRefrence[0]).toInt();
    dateFormat[1] = Date.mid(slashRefrence[0]+1, slashRefrence[1] - (slashRefrence[0] +1)).toInt();
    dateFormat[2] = Date.mid(slashRefrence[1]+1, Date.length()).toInt();

    ExtDate tempdate(dateFormat[2], dateFormat[0], dateFormat[1]);
    if (!tempdate.isValid())
    {
        KMessageBox::error(this, invalidFormatMsg);
        return false;
     }

     // Convert to JD and verify its lower limit
     TempJD = tempdate.jd();

     if (TempJD >= JDCutOff)
        {
          JD->setNum(TempJD, 10);
          return true;
        }
        else
        {
             const char* invalidJD = I18N_NOOP("No data available for JD prior to %d");
             KMessageBox::error(this, QString().sprintf(invalidJD, JDCutOff));
             return false;
        }
  
}

void LCGenerator::DownloadCurve(QString FinalStartDate, QString FinalEndDate, QString FinalDesignation, QString AverageDay)
{

	QString buf(Hostprefix);
	QString Yes("yes");
	QString No("no");
	
	//FainterCheck;
	//CCDVCheck;
	//CCDICheck;
	//CCDRCheck;
	//CCDBCheck;
	//VisualCheck;
	//DiscrepantCheck;
    

	buf.append("?"+FinalStartDate);
	buf.append("?"+FinalEndDate);
	buf.append("?"+FinalDesignation);
	buf.append("?"+AverageDay);
	buf.append("?"+ (FainterCheck->isOn() ? Yes : No));
	buf.append("?"+ (CCDVCheck->isOn() ? Yes : No));
	buf.append("?"+ (CCDICheck->isOn() ? Yes : No));
	buf.append("?"+ (CCDRCheck->isOn() ? Yes : No));
	buf.append("?"+ (CCDBCheck->isOn() ? Yes : No));
	buf.append("?"+ (VisualCheck->isOn() ? Yes : No));
	buf.append("?"+ (DiscrepantCheck->isOn() ? Yes : No));
	

	KURL url(buf);
	QString message = i18n( "Light Curve produced by the American Amateur Variable Star Observers" );
	// parent of imageview is KStars
	new ImageViewer(&url, message, ksw, "lightcurve");
        
}

void LCGenerator::updateDesigList(int index)
{

    DesignationIn->setSelected(index, true);
    DesignationIn->centerCurrentItem();
    
}

void LCGenerator::updateNameList(int index)
{

    NameIn->setSelected(index, true);
    NameIn->centerCurrentItem();
    
}

void LCGenerator::updateStarList()
{
	file->setName( locateLocal( "appdata", "valaav.txt" ) );
	
	KURL AAVSOFile("http://www.aavso.org/observing/aids/valaav.txt");
	KURL saveFile (file->name());
	
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
		
		DesignationIn->clear();
                NameIn->clear();
		
		// Fill stars designations
                for (uint i=0; i< (ksw->data()->VariableStarsList.count()); i++)
                DesignationIn->insertItem(ksw->data()->VariableStarsList.at(i)->Designation);

                // Fill star names
                for (uint i=0; i<ksw->data()->VariableStarsList.count(); i++)
                NameIn->insertItem(ksw->data()->VariableStarsList.at(i)->Name);
		
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
