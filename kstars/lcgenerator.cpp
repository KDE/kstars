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
#include <qvariant.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qfile.h>
#include <qtextstream.h>

#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kmainwindow.h>

#include "lcgenerator.h"
#include "imageviewer.h"
#include "ksutils.h"
#include "kstars.h"

#if (QT_VERSION < 300)
#include <kapp.h>
#else
#include <kapplication.h>
#endif

LCGenerator::LCGenerator( QWidget* parent)
    : QDialog( parent, "lcgenerator", FALSE,  WType_TopLevel) , Hostprefix("http://www.aavso.org/cgi-bin/xephemstar.pl"), JDCutOff(2437600)
{

    // pointer to KStars to access date
    ksw = (KStars*) kapp->mainWidget();
    resize( 455, 370 ); 
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setMinimumSize( QSize( 455, 370 ) );
    setMaximumSize( QSize( 455, 370 ) );
    setCaption( i18n( "AAVSO Light Curve Generator" ) );

    createGUI();
}

LCGenerator::~LCGenerator()
{
    // no need to delete child widgets, Qt does it all for us
}

void LCGenerator::createGUI()
{

    StarInfoBox = new QGroupBox( this, "StarInfoBox" );
    StarInfoBox->setGeometry( QRect( 5, 10, 245, 245 ) );
    StarInfoBox->setTitle( i18n( "Star Info" ) );

    TextLabel1 = new QLabel( StarInfoBox, "TextLabel1" );
    TextLabel1->setGeometry( QRect( 10, 30, 81, 26 ) );
    TextLabel1->setText( i18n( "Designation:" ) );

    TextLabel2 = new QLabel( StarInfoBox, "TextLabel2" );
    TextLabel2->setGeometry( QRect( 10, 55, 81, 16 ) );
    QFont TextLabel2_font(  TextLabel2->font() );
    TextLabel2_font.setPointSize( 9 );
    TextLabel2->setFont( TextLabel2_font );
    TextLabel2->setText( i18n( "Ex: 0214-03" ) );

    TextLabel3 = new QLabel( StarInfoBox, "TextLabel3" );
    TextLabel3->setGeometry( QRect( 10, 75, 81, 26 ) );
    TextLabel3->setText( i18n( "Or Name:" ) );

    TextLabel4 = new QLabel( StarInfoBox, "TextLabel4" );
    TextLabel4->setGeometry( QRect( 10, 100, 81, 16 ) );
    QFont TextLabel4_font(  TextLabel4->font() );
    TextLabel4_font.setPointSize( 9 );
    TextLabel4->setFont( TextLabel4_font );
    TextLabel4->setText( i18n( "Ex: Omi Cet" ) );

    TextLabel5 = new QLabel( StarInfoBox, "TextLabel5" );
    TextLabel5->setGeometry( QRect( 10, 125, 81, 26 ) );
    TextLabel5->setText( i18n( "Start Date:" ) );

    TextLabel6 = new QLabel( StarInfoBox, "TextLabel6" );
    TextLabel6->setGeometry( QRect( 10, 155, 107, 16 ) );
    QFont TextLabel6_font(  TextLabel6->font() );
    TextLabel6_font.setPointSize( 9 );
    TextLabel6->setFont( TextLabel6_font );
    TextLabel6->setText( i18n( "In JD or mm/dd/yyyy" ) );

    TextLabel7 = new QLabel( StarInfoBox, "TextLabel7" );
    TextLabel7->setGeometry( QRect( 10, 180, 81, 26 ) );
    TextLabel7->setText( i18n( "End Date:" ) );

    TextLabel8 = new QLabel( StarInfoBox, "TextLabel8" );
    TextLabel8->setGeometry( QRect( 10, 210, 107, 16 ) );
    QFont TextLabel8_font(  TextLabel8->font() );
    TextLabel8_font.setPointSize( 9 );
    TextLabel8->setFont( TextLabel8_font );
    TextLabel8->setText( i18n( "In JD or mm/dd/yyyy" ) );

    DesignationIn = new QLineEdit( StarInfoBox, "DesignationIn" );
    DesignationIn->setGeometry( QRect( 95, 30, 116, 26 ) );

    NameIn = new QLineEdit( StarInfoBox, "NameIn" );
    NameIn->setGeometry( QRect( 95, 75, 116, 26 ) );

    StartDateIn = new QLineEdit( StarInfoBox, "StartDateIn" );
    StartDateIn->setGeometry( QRect( 95, 125, 116, 26 ) );
    StartDateIn->setText( i18n( "default" ) );

    EndDateIn = new QLineEdit( StarInfoBox, "EndDateIn" );
    EndDateIn->setGeometry( QRect( 95, 180, 116, 26 ) );
    EndDateIn->setText( i18n( "default" ) );

    DataSelectBox = new QGroupBox( this, "DataSelectBox" );
    DataSelectBox->setGeometry( QRect( 255, 10, 195, 245 ) );
    DataSelectBox->setTitle( i18n( "Data Selection" ) );

    CCDVCheck = new QCheckBox( DataSelectBox, "CCDVCheck" );
    CCDVCheck->setGeometry( QRect( 10, 125, 75, 16 ) );
    CCDVCheck->setText( i18n( "CCDV" ) );
    CCDVCheck->setChecked( TRUE );

    CCDRCheck = new QCheckBox( DataSelectBox, "CCDRCheck" );
    CCDRCheck->setGeometry( QRect( 10, 150, 75, 16 ) );
    CCDRCheck->setText( i18n( "CCDR" ) );
    CCDRCheck->setChecked( TRUE );

    CCDICheck = new QCheckBox( DataSelectBox, "CCDICheck" );
    CCDICheck->setGeometry( QRect( 10, 175, 75, 16 ) );
    CCDICheck->setText( i18n( "CCDI" ) );
    CCDICheck->setChecked( TRUE );

    VisualCheck = new QCheckBox( DataSelectBox, "VisualCheck" );
    VisualCheck->setGeometry( QRect( 10, 25, 75, 16 ) );
    VisualCheck->setText( i18n( "Visual" ) );
    VisualCheck->setChecked( TRUE );

    FainterCheck = new QCheckBox( DataSelectBox, "FainterCheck" );
    FainterCheck->setGeometry( QRect( 10, 50, 115, 16 ) );
    FainterCheck->setText( i18n( "Fainter Thans" ) );
    FainterCheck->setChecked( TRUE );

    DiscrepantCheck = new QCheckBox( DataSelectBox, "DiscrepantCheck" );
    DiscrepantCheck->setGeometry( QRect( 10, 75, 130, 16 ) );
    DiscrepantCheck->setText( i18n( "Discrepant Data" ) );

    CCDBCheck = new QCheckBox( DataSelectBox, "CCDBCheck" );
    CCDBCheck->setGeometry( QRect( 10, 100, 75, 16 ) );
    CCDBCheck->setText( i18n( "CCDB" ) );
    CCDBCheck->setChecked( TRUE );

    TextLabel9 = new QLabel( DataSelectBox, "TextLabel9" );
    TextLabel9->setGeometry( QRect( 10, 205, 91, 21 ) );
    TextLabel9->setText( i18n( "Plot Average:" ) );

    AverageDayIn = new QLineEdit( DataSelectBox, "AverageDayIn" );
    AverageDayIn->setGeometry( QRect( 100, 200, 46, 26 ) );

    TextLabel10 = new QLabel( DataSelectBox, "TextLabel10" );
    TextLabel10->setGeometry( QRect( 155, 205, 34, 21 ) );
    TextLabel10->setText( i18n( "days" ) );

    ImageConfBox = new QGroupBox( this, "ImageConfBox" );
    ImageConfBox->setGeometry( QRect( 5, 260, 445, 80 ) );
    ImageConfBox->setTitle( i18n( "Image Configuration" ) );

    TextLabel11 = new QLabel( ImageConfBox, "TextLabel11" );
    TextLabel11->setGeometry( QRect( 10, 30, 45, 16 ) );
    TextLabel11->setText( i18n( "Width:" ) );

    TextLabel12 = new QLabel( ImageConfBox, "TextLabel12" );
    TextLabel12->setGeometry( QRect( 10, 54, 85, 16 ) );
    QFont TextLabel12_font(  TextLabel12->font() );
    TextLabel12_font.setPointSize( 9 );
    TextLabel12->setFont( TextLabel12_font );
    TextLabel12->setText( i18n( "100-2000 pixels" ) );

    TextLabel13 = new QLabel( ImageConfBox, "TextLabel13" );
    TextLabel13->setGeometry( QRect( 135, 30, 45, 16 ) );
    TextLabel13->setText( i18n( "Height:" ) );

    TextLabel14 = new QLabel( ImageConfBox, "TextLabel14" );
    TextLabel14->setGeometry( QRect( 135, 54, 85, 16 ) );
    QFont TextLabel14_font(  TextLabel14->font() );
    TextLabel14_font.setPointSize( 9 );
    TextLabel14->setFont( TextLabel14_font );
    TextLabel14->setText( i18n( "100-1500 pixels" ) );

    ImageWidthIn = new QLineEdit( ImageConfBox, "ImageWidthIn" );
    ImageWidthIn->setGeometry( QRect( 55, 25, 65, 26 ) );
    ImageWidthIn->setText( i18n( "450" ) );

    ImageHeightIn = new QLineEdit( ImageConfBox, "ImageHeightIn" );
    ImageHeightIn->setGeometry( QRect( 185, 25, 65, 26 ) );
    ImageHeightIn->setText( i18n( "375" ) );

    GridGroup = new QButtonGroup( ImageConfBox, "GridGroup" );
    GridGroup->setGeometry( QRect( 300, 15, 66, 55 ) );
    GridGroup->setFrameShape( QButtonGroup::NoFrame );
    GridGroup->setFrameShadow( QButtonGroup::Plain );
    GridGroup->setTitle( QString::null );

    GridOffRad = new QRadioButton( GridGroup, "GridOffRad" );
    GridOffRad->setGeometry( QRect( 9, 9, 55, 16 ) );
    GridOffRad->setText( i18n( "Off" ) );
    GridOffRad->setChecked( TRUE );

    GridOnRad = new QRadioButton( GridGroup, "GridOnRad" );
    GridOnRad->setGeometry( QRect( 9, 29, 55, 16 ) );
    GridOnRad->setText( i18n( "On" ) );

    TextLabel15 = new QLabel( ImageConfBox, "TextLabel15" );
    TextLabel15->setGeometry( QRect( 265, 30, 35, 21 ) );
    TextLabel15->setText( i18n( "Grid:" ) );


    // Buttons
    CloseButton = new QPushButton( this, "CloseButton" );
    CloseButton->setGeometry( QRect( 356, 345, 95, 21 ) );
    CloseButton->setText( i18n( "Close" ) );

    GetCurveButton = new QPushButton( this, "GetCurveButton" );
    GetCurveButton->setGeometry( QRect( 5, 345, 155, 21 ) );
    GetCurveButton->setText( i18n( "Retrieve Curve" ) );
    GetCurveButton->setDefault( TRUE );

    // tab order
    setTabOrder( DesignationIn, NameIn );
    setTabOrder( NameIn, StartDateIn );
    setTabOrder( StartDateIn, EndDateIn );
    setTabOrder( EndDateIn, VisualCheck );
    setTabOrder( VisualCheck, FainterCheck );
    setTabOrder( FainterCheck, DiscrepantCheck );
    setTabOrder( DiscrepantCheck, CCDBCheck );
    setTabOrder( CCDBCheck, CCDVCheck );
    setTabOrder( CCDVCheck, CCDRCheck );
    setTabOrder( CCDRCheck, CCDICheck );
    setTabOrder( CCDICheck, AverageDayIn );
    setTabOrder( AverageDayIn, ImageWidthIn );
    setTabOrder( ImageWidthIn, ImageHeightIn );
    setTabOrder( ImageHeightIn, GridOffRad );
    setTabOrder( GridOffRad, GridOnRad );
    setTabOrder( GridOnRad, GetCurveButton );
    setTabOrder( GetCurveButton, CloseButton );

    // Signals/Slots
    QObject::connect(CloseButton, SIGNAL(clicked()), this, SLOT(close()));
    QObject::connect(GetCurveButton, SIGNAL(clicked()), this, SLOT(VerifyData()));

}

void LCGenerator::VerifyData()
{
    uint i=0;
    QString InitialDesignation, InitialName, InitialStartDate, InitialEndDate;
    QString FinalDesignation, FinalStartDate, FinalEndDate;
    bool starFound = false;

    // Get initial user input
    InitialDesignation  = DesignationIn->text().stripWhiteSpace();
    InitialName           = NameIn->text().simplifyWhiteSpace().upper();
    InitialStartDate     = StartDateIn->text().lower();
    InitialEndDate       = EndDateIn->text().lower();
        
    // #1
    // verify that the star exists in the database

    // Both fields empty
    if (InitialDesignation.isEmpty() && NameIn->text().isEmpty())
    {
       KMessageBox::error(this, i18n("Both Designation and Name fields are empty!"));
       return;
    }

    // Designation filed not empty
    if (!InitialDesignation.isEmpty())
    {
       for (i=0; i<ksw->data()->VariableStarsList.count(); i++)
       {
           if (InitialDesignation == ksw->data()->VariableStarsList.at(i)->Designation)
           {
             FinalDesignation = InitialDesignation;
             starFound = true;
             break;
           }

     }
      
      if (!starFound)
         {
            KMessageBox::error(this,  i18n("Star Designation does not exist in the AAVSO Variable Stars database, please check again."));
            return;
         }
    }      

    // Name Filed not empty
    if (!InitialName.isEmpty() && !starFound)
    {
       for (i=0; i<ksw->data()->VariableStarsList.count(); i++)
       {
           if (InitialName == ksw->data()->VariableStarsList.at(i)->Name)
           {
             FinalDesignation = ksw->data()->VariableStarsList.at(i)->Designation;
             starFound = true;
             break;
           }
       }

      if (!starFound)
      {
         KMessageBox::error(this,  i18n("Star Name does not exist in the AAVSO Variable Stars database, please check again."));
         return;
      }
    }

    // set Julian day
    if (!setJD(InitialStartDate, &FinalStartDate))
        return;
    if (!setJD(InitialEndDate, &FinalEndDate))
        return;
    
    if (FinalEndDate.toInt() < FinalStartDate.toInt())
    {
        KMessageBox::error(this, i18n("End Date must occur after Start Date."));
        return;
    }

    //#3
    // Check for image width, height
    if (ImageWidthIn->text().toInt() < 100 || ImageWidthIn->text().toInt() > 2000)
    {
        KMessageBox::error(this, i18n("Image Width must be in the range of 100 to 2000 pixels."));
        return;
    }

   if (ImageHeightIn->text().toInt() < 150 || ImageHeightIn->text().toInt() > 1500)
   {
       KMessageBox::error(this, i18n("Image Height must be in the range  of 100 to 1500 pixels."));
       return;
   }

  
    //#4 Download the curve!
   DownloadCurve(FinalStartDate, FinalEndDate, FinalDesignation);

}

bool LCGenerator::setJD(QString Date, QString *JD)
{
    uint i=0;
    int TempJD=0;
    int slashCount =0;
    int slashRefrence[2];

    const QString invalidFormatMsg(i18n("Invalid date format. Correct format is mm/dd/yyyy or JD, leave 'default' to generate light curve for the last 300 days."));
    int dateFormat[3];
    bool isNumber;

    // check for "default" date
    if (Date == QString("default"))
    {
       *JD = Date;
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

    QDate tempdate(dateFormat[2], dateFormat[0], dateFormat[1]);
    if (!tempdate.isValid())
    {
        KMessageBox::error(this, invalidFormatMsg);
        return false;
     }

     // Convert to JD and verify its lower limit
     QDateTime datetime( tempdate );

     TempJD =  int(KSUtils::UTtoJulian(datetime));

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

void LCGenerator::DownloadCurve(QString FinalStartDate, QString FinalEndDate, QString FinalDesignation)
{
        QString buf(Hostprefix);

        buf.append("?"+FinalStartDate);
        buf.append("?"+FinalEndDate);
        buf.append("?"+FinalDesignation);
        
        KURL url(buf);
        // parent of imageview is KStars
        new ImageViewer(&url, ksw, "lightcurve");
}

#include "lcgenerator.moc"
