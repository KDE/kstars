/***************************************************************************
                          altvstime.h  -  description
                             -------------------
    begin                : Mon Dec 23 2002
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

#ifndef ALTVSTIME_H
#define ALTVSTIME_H

#include "kstarsplotwidget.h"

class KStarsDateTime;
class QTime;
class QVariant;
class QVBoxLayout;
class KDialogBase;
class KLocale;
class SkyObject;
class SkyPoint;
class dms;
class GeoLocation;
class KStars;
class AltVsTimeUI;

/**@class AVTPlotWidget
	*@short An extension of the KStarsPlotWidget for the AltVsTime tool.
	*The biggest difference is that in addition to the plot objects, it 
	*draws the "ground" below Alt=0 and draws the sky light blue for day 
	*times, and black for night times.  The transition between day and 
	*night is drawn with a gradient, and the position follows the actual 
	*sunrise/sunset times of the given date/location.
	*Also, this plot widget provides two time axes (local time along the 
	*bottom, and local sideral time along the top).
	*@version 1.0
	*@author Jason Harris
	*/
class AVTPlotWidget : public KStarsPlotWidget
{
	Q_OBJECT
public:
/**Constructor
	*/
	AVTPlotWidget( double x1=0.0, double x2=1.0, double y1=0.0, double y2=1.0, QWidget *parent=0, const char* name=0 );

/**Set the fractional positions of the Sunrise and Sunset positions, 
	*in units where last midnight was 0.0, and next midnight is 1.0.  
	*i.e., if Sunrise is at 06:00, then we set it as 0.25 in this 
	*function.  Likewise, if Sunset is at 18:00, then we set it as 
	*0.75 in this function.
	*@param sr the fractional position of Sunrise
	*@param ss the fractional position of Sunset
	*/
	void setSunRiseSetTimes( double sr, double ss ) { SunRise = sr; SunSet = ss; }

protected:
/**Handle mouse move events.  If the mouse button is down,
	*draw crosshair lines centered at the cursor position.  This 
	*allows the user to pinpoint specific position sin the plot.
	*/
	void mouseMoveEvent( QMouseEvent *e );
	
/**Simply calls mouseMoveEvent().
	*/
	void mousePressEvent( QMouseEvent *e );
	
/**Redraw the plot.
	*/
	void paintEvent( QPaintEvent *e );

private: 
	double SunRise, SunSet;
};

/**@class AltVsTime
	*@short the Altitude vs. Time Tool.
	*Plot the altitude as a function of time for any list of 
	*objects, as seen from any location, on any date.
	*@version 1.0
	*@author Jason Harris
	*/

class AltVsTime : public KDialogBase
{
	Q_OBJECT

public:
/**Constructor
	*/
	AltVsTime( QWidget* parent = 0);
	
/**Destructor
	*/
	~AltVsTime();

/**Determine the limits for the sideral time axis, using
	*the sidereal time at midnight for the current date 
	*and location settings.
	*/
	void setLSTLimits();
	
/**Set the AltVsTime Date according to the current Date
	*in the KStars main window.  Currently, this is only 
	*used in the ctor to initialize the Date.
	*/
	void showCurrentDate (void);
	
/**@return a KStarsDateTime object constructed from the 
	*current setting in the Date widget.
	*/
	KStarsDateTime getDate (void);
	
/**Determine the time of sunset and sunrise for the current 
	*date and location settings.  Convert the times to doubles, 
	*expressing the times as fractions of a full day.
	*Calls AVTPlotWidget::setSunRiseSetTimes() to send the 
	*numbers to the plot widget.
	*/
	void computeSunRiseSetTimes();
	
/**Parse a string as an epoch number.  If the string can't 
	*be parsed, return 2000.0.
	*@param eName the epoch string to be parsed
	*@return the epoch number
	*/
	double getEpoch (QString eName);
	
/**@short Add a SkyObject to the display.
	*Constructs a PLotObject representing the Alt-vs-time curve for the object.
	*@param o pointer to the SkyObject to be added
	*@param forceAdd if true, then the object will be added, even if there 
	*is already a curve for the same coordinates.
	*/
	void processObject( SkyObject *o, bool forceAdd=false );
	
/**@short Determine the altitude coordinate of a SkyPoint, 
	*given an hour of the day.
	*
	*This is called for every 30-minute interval in the displayed Day, 
	*in order to construct the altitude curve for a given object.
	*@param p the skypoint whose altitude is to be found
	*@param hour the time in the displayed day, expressed in hours
	*@return the Altitude, expresse in degrees
	*/
	double findAltitude( SkyPoint *p, double hour );
	
/**@return the currently highlighted item in the list of displayed 
	*objects
	*/
	int currentPlotListItem() const;
	
/**@return a pointer to the list of SkyPoints representing the 
	*objects being displayed.
	*/
	QPtrList<SkyPoint>* skyPointList() { return &pList; }

public slots:
/**@short Update the plot to reflec new Date and Location settings.
	*/
	void slotUpdateDateLoc(void);
	
/**@short Clear the list of displayed objects.
	*/
	void slotClear(void);
	
/**@short Clear the edit boxes for specifying a new object.
	*/
	void slotClearBoxes(void);
	
/**@short Add an object to the list of displayed objects, according
	*to the data entered in the edit boxes.
	*/
	void slotAddSource(void);
	
/**@short Launch the Find Object window to select a new object for 
	*the list of displayed objects.
	*/
	void slotBrowseObject(void);
	
/**@short Launch the Location dialog to choose a new location.
	*/
	void slotChooseCity(void);
	
/**@short Move input keyboard focus to the next logical widget.
	*We need a separate slot for this because we are intercepting 
	*Enter key events, which close the window by default, to 
	*advance input focus instead (when the Enter events occur in 
	*certain Edit boxes).
	*/
	void slotAdvanceFocus(void);
	
/**Update the plot to highlight the altitude curve of the objects
	*which is highlighted in the listbox.
	*/
	void slotHighlight(void);

private:
	AVTPlotWidget *View;
	AltVsTimeUI *avtUI;
	QVBoxLayout *topLayout;

	GeoLocation *geo;
	KStars *ks;
	QPtrList<SkyPoint> pList;
	QPtrList<SkyPoint> deleteList;

	int DayOffset;
	bool dirtyFlag;
};

#endif // ALTVSTIME_H
