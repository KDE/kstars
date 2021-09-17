/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QDialog>

#include "ui_altvstime.h"

class QCPAbstractPlottable;
class QCPItemPixmap;
class QCPRange;
class QMouseEvent;
class QPixmap;

class GeoLocation;
class KStarsDateTime;
class SkyObject;
class SkyPoint;

class AltVsTimeUI : public QFrame, public Ui::AltVsTime
{
    Q_OBJECT
  public:
    explicit AltVsTimeUI(QWidget *p = nullptr);
};

/**
 * @class AltVsTime
 * @short the Altitude vs. Time Tool.
 * Plot the altitude as a function of time for any list of
 * objects, as seen from any location, on any date.
 *
 * @author Jason Harris
 */
class AltVsTime : public QDialog
{
    Q_OBJECT

  public:
    /** Constructor */
    explicit AltVsTime(QWidget *parent = nullptr);

    /** Destructor */
    ~AltVsTime() override;

    /**
     * Determine the limits for the sideral time axis, using
     * the sidereal time at midnight for the current date
     * and location settings.
     */
    void setLSTLimits();

    /**
     * Set the AltVsTime Date according to the current Date
     * in the KStars main window.  Currently, this is only
     * used in the ctor to initialize the Date.
     */
    void showCurrentDate();

    /**
     * @return a KStarsDateTime object constructed from the
     * current setting in the Date widget.
     */
    KStarsDateTime getDate();

    /**
     * Determine the time of sunset and sunrise for the current
     * date and location settings.  Convert the times to doubles,
     * expressing the times as fractions of a full day.
     * Calls AVTPlotWidget::setSunRiseSetTimes() to send the
     * numbers to the plot widget.
     */
    void computeSunRiseSetTimes();

    /**
     * Parse a string as an epoch number.  If the string can't
     * be parsed, return 2000.0.
     * @param eName the epoch string to be parsed
     * @return the epoch number
     */
    double getEpoch(const QString &eName);

    /**
     * @short Add a SkyObject to the display.
     * Constructs a PLotObject representing the Alt-vs-time curve for the object.
     * @param o pointer to the SkyObject to be added
     * @param forceAdd if true, then the object will be added, even if there
     * is already a curve for the same coordinates.
     */
    void processObject(SkyObject *o, bool forceAdd = false);

    /**
     * @short Determine the altitude coordinate of a SkyPoint,
     * given an hour of the day.
     *
     * This is called for every 30-minute interval in the displayed Day,
     * in order to construct the altitude curve for a given object.
     * @param p the skypoint whose altitude is to be found
     * @param hour the time in the displayed day, expressed in hours
     * @return the Altitude, expressed in degrees
     */
    double findAltitude(SkyPoint *p, double hour);

    /**
     * @short get object name. If star has no name, generate a name based on catalog number.
     * @param o sky object.
     * @param translated set to true if the translated name is required.
     */
    QString getObjectName(const SkyObject *o, bool translated = true);

    void drawGradient();

  public slots:
    /** @short Update the plot to reflec new Date and Location settings. */
    void slotUpdateDateLoc();

    /** @short Clear the list of displayed objects. */
    void slotClear();

    /** @short Show information from the curve as a tooltip. */
    void plotMousePress(QCPAbstractPlottable *abstractPlottable, int dataIndex, QMouseEvent *event);

    /** @short Update the X axis on Zoom and Drag. */
    void onXRangeChanged(const QCPRange &range);

    /** @short Update the Y axis on Zoom and Drag. */
    void onYRangeChanged(const QCPRange &range);

    /** @short Compute the altitude for a certain time. */
    void slotComputeAltitudeByTime();

    /** @short Mark the rise time on the curve. */
    void slotMarkRiseTime();

    /** @short Mark the set time on the curve. */
    void slotMarkSetTime();

    /** @short Mark the transit time on the curve. */
    void slotMarkTransitTime();

    /** @short Draw the white vertical line on click. */
    void mouseOverLine(QMouseEvent *event);

    /** @short Clear the edit boxes for specifying a new object. */
    void slotClearBoxes();

    /**
     * @short Add an object to the list of displayed objects, according
     * to the data entered in the edit boxes.
     */
    void slotAddSource();

    /**
     * @short Launch the Find Object window to select a new object for
     * the list of displayed objects.
     */
    void slotBrowseObject();

    /** @short Launch the Location dialog to choose a new location. */
    void slotChooseCity();

    /**
     * @short Move input keyboard focus to the next logical widget.
     * We need a separate slot for this because we are intercepting
     * Enter key events, which close the window by default, to
     * advance input focus instead (when the Enter events occur in
     * certain Edit boxes).
     */
    void slotAdvanceFocus();

    /**
     * Update the plot to highlight the altitude curve of the objects
     * which is highlighted in the listbox.
     */
    void slotHighlight(int);

    /** @short Print plot widget */
    void slotPrint();

  private:
    /** @short find start of dawn, end of dusk, maximum and minimum elevation of the sun */
    void setDawnDusk();

    AltVsTimeUI *avtUI { nullptr };

    GeoLocation *geo { nullptr };
    QList<SkyObject *> pList;
    QList<SkyObject *> deleteList;
    int DayOffset { 0 };
    int minAlt { 0 };
    int maxAlt { 0 };
    QCPItemPixmap *background { nullptr };
    QPixmap *gradient { nullptr };
};
