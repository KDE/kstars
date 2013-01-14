/***************************************************************************
                          modcalcdaylength.h  -  description
                             -------------------
    begin                : wed jun 12 2002
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

#ifndef MODCALCDAYLENGTH_H_
#define MODCALCDAYLENGTH_H_

#include "ui_modcalcdaylength.h"

class GeoLocation;
class QTextStream;

/** Module to compute the equatorial coordinates for a given date and time
 * from a given epoch or equinox
  *@author Pablo de Vicente
  */
class modCalcDayLength : public QFrame, public Ui::modCalcDayLengthDlg  {
    Q_OBJECT
public:
    /**Constructor. */
    explicit modCalcDayLength(QWidget *p);
    /**Destructor. */
    ~modCalcDayLength();

public slots:
    void slotLocation();
    void slotLocationBatch();
    void slotComputeAlmanac();
    void slotRunBatch();
    void slotViewBatch();
    void slotCheckFiles();

private:
    void updateAlmanac( const QDate &d, GeoLocation *geo );
    QTime lengthOfDay(QTime setQTime, QTime riseQTime);

    void showCurrentDate(void);
    void initGeo(void);
    void processLines( QTextStream &istream );

    GeoLocation *geoPlace, *geoBatch;
    QString srTimeString, stTimeString, ssTimeString;
    QString mrTimeString, mtTimeString, msTimeString;
    QString srAzString, stAltString, ssAzString;
    QString mrAzString, mtAltString, msAzString;
    QString daylengthString, lunarphaseString;
};

#endif
