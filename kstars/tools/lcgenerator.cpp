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

#include <tqvariant.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>
#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqlabel.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tqtooltip.h>
#include <tqwhatsthis.h>
#include <tqfile.h>

#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "lcgenerator.h"
#include "imageviewer.h"
#include "kstars.h"
#include "kstarsdata.h"

#include <kapplication.h>

LCGenerator::LCGenerator( TQWidget* parent)
    : KDialogBase( parent, "lcgenerator", false, i18n( "AAVSO Light Curve Generator"),0) , Hostprefix("http://www.aavso.org/cgi-bin/kstar.pl"), JDCutOff(2437600)
{

  ksw = (KStars*) parent;
  createGUI();
  
  downloadJob = 0;
  
  file = new TQFile();
}

LCGenerator::~LCGenerator()
{
   delete file;
   delete downloadJob;
}

void LCGenerator::createGUI()
{

    TQWidget *page = new TQWidget(this);
    setMainWidget(page);
    
    LCGeneratorDialogLayout = new TQVBoxLayout( page, 11, 6, "LCGeneratorDialogLayout"); 

    SDLayout = new TQHBoxLayout( 0, 0, 6, "SDLayout"); 

    StarInfoBox = new TQGroupBox( page, "StarInfoBox" );
    StarInfoBox->setColumnLayout(0, Qt::Vertical );
    StarInfoBox->layout()->setSpacing( 6 );
    StarInfoBox->layout()->setMargin( 11 );
    StarInfoBoxLayout = new TQVBoxLayout( StarInfoBox->layout() );
    StarInfoBoxLayout->setAlignment( Qt::AlignTop );

    DesignHLayout = new TQHBoxLayout( 0, 0, 6, "DesignHLayout"); 

    desigLabel = new TQLabel( StarInfoBox, "desigLabel" );
    desigLabel->setMinimumSize( TQSize( 70, 0 ) );
    DesignHLayout->addWidget( desigLabel );

    DesignationIn = new KListBox( StarInfoBox, "DesignationIn" );
    DesignHLayout->addWidget( DesignationIn );
    StarInfoBoxLayout->addLayout( DesignHLayout );
    
    // Fill stars designations
    for (uint i=0; i< (ksw->data()->VariableStarsList.count()); i++)
     DesignationIn->insertItem(ksw->data()->VariableStarsList.at(i)->Designation);

    NameHLayout = new TQHBoxLayout( 0, 0, 6, "NameHLayout"); 

    nameLabel = new TQLabel( StarInfoBox, "nameLabel" );
    nameLabel->setMinimumSize( TQSize( 70, 0 ) );
    NameHLayout->addWidget( nameLabel );

    NameIn = new KListBox( StarInfoBox, "NameIn" );
    NameHLayout->addWidget( NameIn );
    StarInfoBoxLayout->addLayout( NameHLayout );
    
    // Fill star names
    for (uint i=0; i<ksw->data()->VariableStarsList.count(); i++)
     NameIn->insertItem(ksw->data()->VariableStarsList.at(i)->Name);

    StartHLayout = new TQHBoxLayout( 0, 0, 6, "StartHLayout"); 

    startLabel = new TQLabel( StarInfoBox, "startLabel" );
    startLabel->setMinimumSize( TQSize( 70, 0 ) );
    StartHLayout->addWidget( startLabel );

    StartDateIn = new KLineEdit( StarInfoBox, "StartDateIn" );
    StartHLayout->addWidget( StartDateIn );
    StarInfoBoxLayout->addLayout( StartHLayout );

    EndHLayout = new TQHBoxLayout( 0, 0, 6, "EndHLayout"); 

    endLabel = new TQLabel( StarInfoBox, "endLabel" );
    endLabel->setMinimumSize( TQSize( 70, 0 ) );
    EndHLayout->addWidget( endLabel );

    EndDateIn = new KLineEdit( StarInfoBox, "EndDateIn" );
    EndHLayout->addWidget( EndDateIn );
    StarInfoBoxLayout->addLayout( EndHLayout );
    SDLayout->addWidget( StarInfoBox );

    DataSelectBox = new TQGroupBox( page, "DataSelectBox" );
    DataSelectBox->setColumnLayout(0, Qt::Vertical );
    DataSelectBox->layout()->setSpacing( 6 );
    DataSelectBox->layout()->setMargin( 11 );
    DataSelectBoxLayout = new TQVBoxLayout( DataSelectBox->layout() );
    DataSelectBoxLayout->setAlignment( Qt::AlignTop );

    VisualCheck = new TQCheckBox( DataSelectBox, "VisualCheck" );
    VisualCheck->setChecked( TRUE );
    DataSelectBoxLayout->addWidget( VisualCheck );

    FainterCheck = new TQCheckBox( DataSelectBox, "FainterCheck" );
    FainterCheck->setChecked( TRUE );
    DataSelectBoxLayout->addWidget( FainterCheck );

    DiscrepantCheck = new TQCheckBox( DataSelectBox, "DiscrepantCheck" );
    DataSelectBoxLayout->addWidget( DiscrepantCheck );

    CCDBCheck = new TQCheckBox( DataSelectBox, "CCDBCheck" );
    CCDBCheck->setChecked( TRUE );
    DataSelectBoxLayout->addWidget( CCDBCheck );

    CCDVCheck = new TQCheckBox( DataSelectBox, "CCDVCheck" );
    CCDVCheck->setChecked( TRUE );
    DataSelectBoxLayout->addWidget( CCDVCheck );

    CCDRCheck = new TQCheckBox( DataSelectBox, "CCDRCheck" );
    CCDRCheck->setChecked( TRUE );
    DataSelectBoxLayout->addWidget( CCDRCheck );

    CCDICheck = new TQCheckBox( DataSelectBox, "CCDICheck" );
    CCDICheck->setChecked( TRUE );
    DataSelectBoxLayout->addWidget( CCDICheck );

    PlotHLayout = new TQHBoxLayout( 0, 0, 6, "PlotHLayout"); 

    plotLabel = new TQLabel( DataSelectBox, "plotLabel" );
    PlotHLayout->addWidget( plotLabel );

    AverageDayIn = new KLineEdit( DataSelectBox, "AverageDayIn" );
    PlotHLayout->addWidget( AverageDayIn );

    daysLabel = new TQLabel( DataSelectBox, "daysLabel" );
    PlotHLayout->addWidget( daysLabel );
    DataSelectBoxLayout->addLayout( PlotHLayout );
    SDLayout->addWidget( DataSelectBox );
    LCGeneratorDialogLayout->addLayout( SDLayout );

    ButtonHLayout = new TQHBoxLayout( 0, 0, 6, "ButtonHLayout"); 

    GetCurveButton = new KPushButton( page, "GetCurveButton" );
    ButtonHLayout->addWidget( GetCurveButton );

    UpdateListButton = new KPushButton( page, "UpdateListButton" );
    ButtonHLayout->addWidget( UpdateListButton );
    TQSpacerItem* spacer = new TQSpacerItem( 128, 16, TQSizePolicy::Expanding, TQSizePolicy::Minimum );
    ButtonHLayout->addItem( spacer );

    CloseButton = new KPushButton( page, "closeButton" );
    ButtonHLayout->addWidget( CloseButton );
    LCGeneratorDialogLayout->addLayout( ButtonHLayout );
    
    
    StarInfoBox->setTitle( i18n( "Star Info" ) );
    desigLabel->setText( i18n( "Designation:" ) );
    nameLabel->setText( i18n( "Or name:" ) );
    startLabel->setText( i18n( "Start date:" ) );
    TQWhatsThis::add( startLabel, i18n( "Start date for the light curve plot in mm/dd/yy or JD" ) );
    endLabel->setText( i18n( "End date:" ) );
    TQWhatsThis::add( endLabel, i18n( "End date for the light curve plot in mm/dd/yy or JD" ) );
    StartDateIn->setText( i18n( "default" ) );
    EndDateIn->setText( i18n( "default" ) );
    DataSelectBox->setTitle( i18n( "Data Selection" ) );
    VisualCheck->setText( i18n( "Visual" ) );
    FainterCheck->setText( i18n( "Fainter thans" ) );
    DiscrepantCheck->setText( i18n( "Discrepant data" ) );
    CCDBCheck->setText( i18n( "CCDB" ) );
    CCDVCheck->setText( i18n( "CCDV" ) );
    CCDRCheck->setText( i18n( "CCDR" ) );
    CCDICheck->setText( i18n( "CCDI" ) );
    plotLabel->setText( i18n( "Plot average:" ) );
    daysLabel->setText( i18n( "days" ) );
    GetCurveButton->setText( i18n( "Retrieve Curve" ) );
    UpdateListButton->setText( i18n( "Update List" ) );
    CloseButton->setText( i18n( "Close" ) );
    
    resize( TQSize(500, 360) );
        
    // Signals/Slots
    TQObject::connect(CloseButton, TQT_SIGNAL(clicked()), this, TQT_SLOT(close()));
    TQObject::connect(GetCurveButton, TQT_SIGNAL(clicked()), this, TQT_SLOT(VerifyData()));
    TQObject::connect(UpdateListButton, TQT_SIGNAL(clicked()), this, TQT_SLOT(updateStarList()));
    TQObject::connect(DesignationIn, TQT_SIGNAL(highlighted(int)), this, TQT_SLOT(updateNameList(int)));
    TQObject::connect(NameIn, TQT_SIGNAL(highlighted(int)), this, TQT_SLOT(updateDesigList(int)));
      
    
}

void LCGenerator::VerifyData()
{
    TQString InitialStartDate, InitialEndDate;
    TQString FinalDesignation, FinalStartDate, FinalEndDate, AverageDays;
    bool AverageDaysOK;

    // Get initial user input
    if ( StartDateIn->text().isEmpty() ) StartDateIn->setText( i18n( "default" ) );
    if ( EndDateIn->text().isEmpty() ) EndDateIn->setText( i18n( "default" ) );
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
   else AverageDays = TQString("default");

    //Download the curve!
   DownloadCurve(FinalStartDate, FinalEndDate, FinalDesignation, AverageDays);
  
}

bool LCGenerator::setJD(TQString Date, TQString *JD, int JDType)
{
    uint i=0;
    int TempJD=0;
    int slashCount =0;
    int slashRefrence[2];

    int dateFormat[3];
    bool isNumber;
    
    const TQString invalidFormatStartJD(i18n("Invalid date format. Correct format is mm/dd/yyyy or JD, leave 'default' to generate light curves for the past 500 days."));
    const TQString invalidFormatENDJD(i18n("Invalid date format. Correct format is mm/dd/yyyy or JD, leave 'default' to generate light curves until today."));
    TQString invalidFormatMsg(JDType ? invalidFormatENDJD : invalidFormatStartJD);
    

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
             KMessageBox::error(this, TQString().sprintf(invalidJD, JDCutOff));
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
             KMessageBox::error(this, TQString().sprintf(invalidJD, JDCutOff));
             return false;
        }
  
}

void LCGenerator::DownloadCurve(TQString FinalStartDate, TQString FinalEndDate, TQString FinalDesignation, TQString AverageDay)
{

	TQString buf(Hostprefix);
	TQString Yes("yes");
	TQString No("no");
	
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
	TQString message = i18n( "Light Curve produced by the American Amateur Variable Star Observers" );
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
	connect (downloadJob, TQT_SIGNAL (result (KIO::Job *)), TQT_SLOT (downloadReady (KIO::Job *)));
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

void LCGenerator::closeEvent (TQCloseEvent *ev)
{
	if (ev)	// not if closeEvent (0) is called, because segfault
		ev->accept();	// parent-widgets should not get this event, so it will be filtered
	//this->~LCGenerator();	// destroy the object, so the object can only allocated with operator new, not as a global/local variable
}

#include "lcgenerator.moc"
