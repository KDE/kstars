/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcsidtime.h"

#include <QFrame>

class QTime;

class GeoLocation;

/**
 * Class which implements the KStars calculator module to compute Universal
 * time to/from Sidereal time.
 *
 * Inherits modCalcSidTimeDlg
 *
 * @author Pablo de Vicente
 * @version 0.9
 */
class modCalcSidTime : public QFrame, public Ui::modCalcSidTimeDlg
{
    Q_OBJECT

  public:
    explicit modCalcSidTime(QWidget *p);

  private slots:
    void slotChangeLocation();
    void slotChangeDate();
    void slotConvertST(const QTime &lt);
    void slotConvertLT(const QTime &st);

    void slotDateChecked();
    void slotLocationChecked();
    void slotLocationBatch();
    void slotCheckFiles();
    void slotRunBatch();
    void slotViewBatch();
    void slotHelpLabel();
    void processLines(QTextStream &istream);

  private:
    /**
     * Fills the UT, Date boxes with the current time
     * and date and the longitude box with the current Geo location
     */
    void showCurrentTimeAndLocation();

    QTime computeLTtoST(QTime lt);
    QTime computeSTtoLT(QTime st);

    GeoLocation *geo { nullptr };
    GeoLocation *geoBatch { nullptr };
};
