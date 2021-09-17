/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcgeod.h"

#include <memory>

class QTextStream;
class GeoLocation;

/**
 * Class which implements the KStars calculator module to compute
 * Geodetic coordinates to/from Cartesian coordinates.
 *
 * @author Pablo de Vicente
 * @version 0.9
 */
class modCalcGeodCoord : public QFrame, public Ui::modCalcGeodCoordDlg
{
    Q_OBJECT
  public:
    explicit modCalcGeodCoord(QWidget *p);

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

    std::unique_ptr<GeoLocation> geoPlace;
    bool xyzInputCoords { false };
};
