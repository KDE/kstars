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

/**
	*@class LCGenerator
	*@short KStars Light Curve Generator
	*This class provides a simple interface that enables a user to download
	*variable stars light curves from an online AAVSO database given few basic parameters.
	*@author Jasem Mutlaq
	*@version 1.0
	*/

#ifndef LCGENERATOR_H
#define LCGENERATOR_H

#include <tqvariant.h>
#include <kdialogbase.h>
#include <kio/job.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QFile;
class KLineEdit;
class KListBox;
class KPushButton;
class QCheckBox;
class QGroupBox;
class QLabel;
class QListBoxItem;

class KStars;

struct VariableStarInfo
{
  TQString Name;
  TQString Designation;
};

class LCGenerator : public KDialogBase
{ 
Q_OBJECT

public:
/**Constructor 
	*@p parent pointer to the parent widget
	*/
	LCGenerator( TQWidget* parent = 0);
/**Destructor */
	~LCGenerator();

private:

/** Initilizes and positions the dialog child widgets. */
	void createGUI();

/** Converts date Julian days, unless date is 'default'.
	*@param date The date to be converted
	*@param *JD pointer to a Julian Day string
	*@param JDType start or end JD
	*@returns true if conversion is successful
	*/
	bool setJD(TQString date, TQString * JD, int JDType);

/** Parses star information and connects to the AAVSO server with the information embedded in the URL
	*@param FinalStartDate The start date in Julian days
	*@param FinalEndDate The end date in Julian days
	*@param FinalDesignation The AAVSO star designation
	*@param AverageDay Number of average days for binning the light curve
	*/
	void DownloadCurve(TQString FinalStartDate, TQString FinalEndDate, TQString FinalDesignation, TQString AverageDay);


	KStars *ksw;
	const TQString Hostprefix;
	const int JDCutOff;
	
	TQGroupBox* StarInfoBox;
	TQLabel* desigLabel;
	KListBox* DesignationIn;
	TQLabel* nameLabel;
	KListBox* NameIn;
	TQLabel* startLabel;
	KLineEdit* StartDateIn;
	TQLabel* endLabel;
	KLineEdit* EndDateIn;
	TQGroupBox* DataSelectBox;
	TQCheckBox* VisualCheck;
	TQCheckBox* FainterCheck;
	TQCheckBox* DiscrepantCheck;
	TQCheckBox* CCDBCheck;
	TQCheckBox* CCDVCheck;
	TQCheckBox* CCDRCheck;
	TQCheckBox* CCDICheck;
	TQLabel* plotLabel;
	KLineEdit* AverageDayIn;
	TQLabel* daysLabel;
	KPushButton* GetCurveButton;
	KPushButton* UpdateListButton;
	KPushButton* CloseButton;

	TQVBoxLayout* LCGeneratorDialogLayout;
	TQHBoxLayout* SDLayout;
	TQVBoxLayout* StarInfoBoxLayout;
	TQHBoxLayout* DesignHLayout;
	TQHBoxLayout* NameHLayout;
	TQHBoxLayout* StartHLayout;
	TQHBoxLayout* EndHLayout;
	TQVBoxLayout* DataSelectBoxLayout;
	TQHBoxLayout* PlotHLayout;
	TQHBoxLayout* ButtonHLayout; 
	
	
	KIO::Job *downloadJob;  // download job of image -> 0 == no job is running
	
	TQFile *file;
	
/**Make sure all events have been processed before closing the dialog 
	*@p ev pointer to the TQCloseEvent object
	*/
	void closeEvent (TQCloseEvent *ev);

public slots:
/** Checks if a star name or designation exists in the database, 
	*verifies date format, and connects to server if no errors occur  
	*/
	void VerifyData();

/**Select the star name that matches the current star designation 
	*@p index the index of the selected designation
	*/
	void updateNameList(int index);

/**Select the star designation that matches the current star name 
	*@p index the index of the selected star name
	*/
	void updateDesigList(int index);

/** Connects to AAVSO database server and downloads a fresh list of Variable stars.*/
	void updateStarList();

/** Reload file and update lists after download */
	void downloadReady(KIO::Job *);
};

#endif // LCGENERATOR_H
