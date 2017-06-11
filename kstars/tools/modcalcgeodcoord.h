/***************************************************************************
                          modcalcgeodcoord.h  -  description
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

#ifndef MODCALCGEODCOORD_H_
#define MODCALCGEODCOORD_H_

#include "ui_modcalcgeod.h"

class QTextStream;
class GeoLocation;

/**
  * Class which implements the KStars calculator module to compute
  * Geodetic coordinates to/from Cartesian coordinates.
  *
  *@author Pablo de Vicente
	*@version 0.9
  */
class modCalcGeodCoord : public QFrame, public Ui::modCalcGeodCoordDlg
{
    Q_OBJECT
  public:
    modCalcGeodCoord(QWidget *p);
    ~modCalcGeodCoord();

    void genGeoCoords(void);
    void getCartGeoCoords(void);
    void getSphGeoCoords(void);
    void showSpheGeoCoords(void);
    void showCartGeoCoords(void);

  public slots:

    void slotComputeGeoCoords(void);
    void slotClearGeoCoords(void);
    void setEllipsoid(int i);
    void slotLongCheckedBatch();
    void slotLatCheckedBatch();
    void slotElevCheckedBatch();
    void slotXCheckedBatch();
    void slotYCheckedBatch();
    void slotZCheckedBatch();
    void slotOutputFile();
    void slotInputFile();

  private:
    void geoCheck(void);
    void xyzCheck(void);
    void showLongLat(void);
    void processLines(QTextStream &istream);
    void slotRunBatch(void);

    //		QRadioButton *cartRadio, *spheRadio;
    //		QVBox *vbox, *rightBox;
    //		QLineEdit *xGeoName, *yGeoName, *zGeoName, *altGeoName;
    //		dmsBox *timeBox, *dateBox, *lonGeoBox, *latGeoBox;

    GeoLocation *geoPlace;
    bool xyzInputCoords;
};

#endif
