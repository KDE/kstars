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

 /** This class provides a simple interface that enables a user to download
   *variable stars light curves from an online AAVSO database given few basic parameters.
   *@short KStars Light Curve Generator
   *@author Jasem Mutlaq
   *@version 0.9
   */
   
#ifndef LCGENERATOR_H
#define LCGENERATOR_H

#include <qvariant.h>
#include <kdialogbase.h>
#include <qfile.h>
#include <kio/job.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
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
  QString Name;
  QString Designation;
};

class LCGenerator : public KDialogBase
{ 
    Q_OBJECT

public:
    LCGenerator( QWidget* parent = 0);
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
    bool setJD(QString date, QString * JD, int JDType);

   /** Parses star information and connects to the AAVSO server with the information embedded in the URL
   *@param FinalStartDate The start date in Julian days
   *@param FinalEndDate The end date in Julian days
   *@param FinalDesignation The AAVSO star designation
   *@param AverageDay Number of average days for binning the light curve
   */
    void DownloadCurve(QString FinalStartDate, QString FinalEndDate, QString FinalDesignation, QString AverageDay);


    KStars *ksw;
    const QString Hostprefix;
    const int JDCutOff;
    
    QGroupBox* StarInfoBox;
    QLabel* desigLabel;
    KListBox* DesignationIn;
    QLabel* nameLabel;
    KListBox* NameIn;
    QLabel* startLabel;
    KLineEdit* StartDateIn;
    QLabel* endLabel;
    KLineEdit* EndDateIn;
    QGroupBox* DataSelectBox;
    QCheckBox* VisualCheck;
    QCheckBox* FainterCheck;
    QCheckBox* DiscrepantCheck;
    QCheckBox* CCDBCheck;
    QCheckBox* CCDVCheck;
    QCheckBox* CCDRCheck;
    QCheckBox* CCDICheck;
    QLabel* plotLabel;
    KLineEdit* AverageDayIn;
    QLabel* daysLabel;
    KPushButton* GetCurveButton;
    KPushButton* UpdateListButton;
    KPushButton* CloseButton;

    QVBoxLayout* LCGeneratorDialogLayout;
    QHBoxLayout* SDLayout;
    QVBoxLayout* StarInfoBoxLayout;
    QHBoxLayout* DesignHLayout;
    QHBoxLayout* NameHLayout;
    QHBoxLayout* StartHLayout;
    QHBoxLayout* EndHLayout;
    QVBoxLayout* DataSelectBoxLayout;
    QHBoxLayout* PlotHLayout;
    QHBoxLayout* ButtonHLayout; 
    
    
    KIO::Job *downloadJob;  // download job of image -> 0 == no job is running
    
    QFile *file;
    
    /**Make sure all events have been processed before closing the dialog */
    void closeEvent (QCloseEvent *ev);
    
    public slots:
    /** Checks if a star name or designation exists in the database, verifies date format, and connects to server if no errors occur  */
    void VerifyData();
    /** Selects a star name based on the star designation */
    void updateNameList(int index);
    /** Selects a designation based on the star name */
    void updateDesigList(int index);
    /** Connects to AAVSO database server and downloads a fresh list of Variable stars.*/
    void updateStarList();
    /** Reload file and update lists after download */
    void downloadReady(KIO::Job *);
    
    
    

};

#endif // LCGENERATOR_H
