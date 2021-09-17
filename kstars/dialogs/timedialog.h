/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdatetime.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QVariant>
#include <QVBoxLayout>

class QHBoxLayout;
class QVBoxLayout;
class KDatePicker;
class QTimeEdit;
class QPushButton;
class GeoLocation;

/**
 * @class TimeDialog
 *
 * A class for adjusting the Time and Date.  Contains a KDatePicker widget
 * for selecting the date, and a QTimeEdit for selecting the time.  There
 * is also a "Now" button for selecting the Time and Date from the system clock.
 *
 *
 * @short Dialog for adjusting the Time and Date.
 * @author Jason Harris
 * @version 1.0
 */
class TimeDialog : public QDialog
{
    Q_OBJECT
  public:
    /**
     * Constructor. Creates widgets and packs them into QLayouts.
     * Connects Signals and Slots.
     */
    TimeDialog(const KStarsDateTime &now, GeoLocation *_geo, QWidget *parent, bool UTCFrame = false);

    ~TimeDialog() override = default;

    /** @returns a QTime object with the selected time */
    QTime selectedTime(void);

    /** @returns a QDate object with the selected date */
    QDate selectedDate(void);

    /** @returns a KStarsDateTime object with the selected date and time */
    KStarsDateTime selectedDateTime(void);

  public slots:
    /**
     * When the "Now" button is pressed, read the time and date from the system clock.
     * Change the selected date in the KDatePicker to the system's date, and the displayed time
     * to the system time.
     */
    void setNow(void);

  protected:
    void keyReleaseEvent(QKeyEvent *) override;

  private:
    bool UTCNow;
    QHBoxLayout *hlay { nullptr };
    QVBoxLayout *vlay { nullptr };
    KDatePicker *dPicker { nullptr };
    QTimeEdit *tEdit { nullptr };
    QPushButton *NowButton { nullptr };
    GeoLocation *geo { nullptr };
};
