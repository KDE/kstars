/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcdaylength.h"

#include <QFrame>

class QDate;
class QTextStream;
class QTime;

class GeoLocation;

/**
 * Module to compute the equatorial coordinates for a given date and time
 * from a given epoch or equinox
 *
 * @author Pablo de Vicente
 */
class modCalcDayLength : public QFrame, public Ui::modCalcDayLengthDlg
{
    Q_OBJECT

  public:
    explicit modCalcDayLength(QWidget *p);

    virtual ~modCalcDayLength() override = default;

  public slots:
    void slotLocation();
    void slotLocationBatch();
    void slotComputeAlmanac();
    void slotRunBatch();
    void slotViewBatch();
    void slotCheckFiles();

  private:
    void updateAlmanac(const QDate &d, GeoLocation *geo);
    QTime lengthOfDay(const QTime &setQTime, const QTime &riseQTime);

    void showCurrentDate();
    void initGeo();
    void processLines(QTextStream &istream);

    GeoLocation *geoPlace { nullptr };
    GeoLocation *geoBatch { nullptr };
    QString srTimeString, stTimeString, ssTimeString;
    QString mrTimeString, mtTimeString, msTimeString;
    QString srAzString, stAltString, ssAzString;
    QString mrAzString, mtAltString, msAzString;
    QString daylengthString, lunarphaseString;
};
