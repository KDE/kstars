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

class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QButtonGroup;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QFrame;

class KListBox;
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
   *@returns true if conversion is successful
   */
    bool setJD(QString date, QString * JD);

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
    
     // Star Info Box
    QGroupBox* StarInfoBox;
    KListBox* DesignationIn;
    KListBox* NameIn;
    QLineEdit* StartDateIn;
    QLineEdit* EndDateIn;
    QLabel* TextLabel1;
    QLabel* TextLabel2;
    QLabel* TextLabel3;
    QLabel* TextLabel4;
    QLabel* TextLabel5;
    QLabel* TextLabel6;
    QLabel* TextLabel7;
    QLabel* TextLabel8;

    // Data Selection Box
    QGroupBox* DataSelectBox;
    QCheckBox* CCDVCheck;
    QCheckBox* CCDRCheck;
    QCheckBox* CCDICheck;
    QCheckBox* VisualCheck;
    QCheckBox* FainterCheck;
    QCheckBox* DiscrepantCheck;
    QCheckBox* CCDBCheck;
    QLineEdit* AverageDayIn;
    QLabel* TextLabel9;
    QLabel* TextLabel10;

    // Image Configuation Box
    QGroupBox* ImageConfBox;
    QLineEdit* ImageWidthIn;
    QLineEdit* ImageHeightIn;
    QButtonGroup* GridGroup;
    QRadioButton* GridOffRad;
    QRadioButton* GridOnRad;
    QLabel* TextLabel15;
    QPushButton* CloseButton;
    QPushButton* GetCurveButton;
    QLabel* TextLabel11;
    QLabel* TextLabel12;
    QLabel* TextLabel13;
    QLabel* TextLabel14;

    public slots:
    /** Checks if a star name or designation exists in the database, verifies date format, and connects to server if no errors occur  */
    void VerifyData();
    /** Selects a star name based on the star designation */
    void updateNameList(int index);
    /** Selects a designation based on the star name */
    void updateDesigList(int index);
    

};

#endif // LCGENERATOR_H
