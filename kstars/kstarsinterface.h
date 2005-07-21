/***************************************************************************
                          kstarsinterface.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu Jan 3 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/




#ifndef KSTARSINTERFACE_H
#define KSTARSINTERFACE_H

#include <dcopobject.h>

/**@class KStarsInterface
	*This class encapsulates the DCOP functions for KStars.
	*@note Clock-related DCOP functions are in a separate class: SimClockInterface
	*@note The function definitions are in the file kstarsdcop.cpp
	*@author Mark Hollomon
	*@version 1.0
	*/


class KStarsInterface : virtual public DCOPObject
{
	K_DCOP

	k_dcop:
		/**Recenter the display at a new object or point in the sky.
			*@param direction This is either the name of a SkyObject, or one 
			*of the following to center on a compass point along the horizon 
			*or at the zenith point:
			*@li north, n
			*@li northeast, ne 
			*@li east, e
			*@li southeast, se 
			*@li south, s
			*@li southwest, sw 
			*@li west, w
			*@li northwest, nw 
			*@li zenith, z
			*
			*/
		virtual ASYNC lookTowards( const QString direction ) = 0;
		
		/**Set the zoomFactor.  The zoomFactor is equal to the number of 
			*pixels which subtend one radian of angle.
			*@param f the new zoomFactor
			*/
		virtual ASYNC zoom( double f ) = 0;
		
		/**Increase the zoomFactor by 10%
			*/
		virtual ASYNC zoomIn() = 0;
		
		/**Decrease the zoomFactor by 10%
			*/
		virtual ASYNC zoomOut() = 0;
		
		/**Set the zoomFactor to its default value
			*/
		virtual ASYNC defaultZoom() = 0;
		
		/**Recenter the Display to the given RA,Dec position
			*@param ra the new RA coordinate
			*@param dec the new Dec coordinate
			*/
		virtual ASYNC setRaDec( double ra, double dec ) = 0;
		
		/**Recenter the Display to the given Alt,Az position
			*@param alt the new Alt coordinate
			*@param az the new Az coordinate
			*/
		virtual ASYNC setAltAz(double alt, double az) = 0;
		
		/**Reset the clock to the given time and date
			*@param yr  the year
			*@param mth the month
			*@param day the day
			*@param hr  the hour
			*@param min the minute
			*@param sec the second
			*/
		virtual ASYNC setLocalTime(int yr, int mth, int day, int hr, int min, int sec) = 0;
		
		/**Pause execution of the script for a given number of seconds
			*@param t pause interval in seconds 
			*/
		virtual ASYNC waitFor( double t ) = 0;
		
		/**Pause execution of the script until a key is pressed
			*@param k the key which will resume the script 
			*/
		virtual ASYNC waitForKey( const QString k ) = 0;
		
		/**Turn tracking mode on or off.  If tracking is on, then the coordinates at 
			*the center of the screen remain fixed with time.  If tracking is off, then
			*the sky "drifts" past the screen at the sidereal rate.
			*@param track if TRUE, turn tracking on; otherwise turn it off.
			*/
		virtual ASYNC setTracking( bool track ) = 0;
		
		/**@short read in the config file
			*/
		virtual ASYNC readConfig() = 0;

		/**@short write current settings to the config file
			*/
		virtual ASYNC writeConfig() = 0;

		/**@return the value of an option in the config file
			*@param name the name of the option to be read
			*/
		virtual QString getOption( const QString &name ) = 0;

		/**Reset a View option.  There are dozens of view options which can be adjusted 
			*with this function.  See the ScriptBuilder tool for a hierarchical list, or
			*see the kstarsrc config file.  Different options require different data types
			*for their argument.  The value parameter will be recast from a QString to the 
			*correct data type for the specified option.  If the value cannot be recast, 
			*then the option will not be changed.
			*@param option the name of the option to change
			*@param value the new value for the option
			*/
		virtual ASYNC changeViewOption( const QString option, const QString value ) = 0;
		
		/**Show a message in a popup window (NOT YET IMPLEMENTED)
			*@param x the X-coordinate of the window
			*@param y the Y-coordinate of the window
			*@param message the text to be displayed
			*/
		virtual ASYNC popupMessage( int x, int y, const QString message ) = 0;
		
		/**Draw a line on the sky map (NOT YET IMPLEMENTED)
			*@param x1 the x-coordinate of the starting point of the line
			*@param y1 the y-coordinate of the starting point of the line
			*@param x2 the x-coordinate of the ending point of the line
			*@param y2 the y-coordinate of the ending point of the line
			*@param speed how fast the line should be drawn from the starting point to the 
			*ending point.  A speed of 0 will draw the entire line instantly.
			*/
		virtual ASYNC drawLine( int x1, int y1, int x2, int y2, int speed ) = 0;
		
		/**Set the Geographic location according to the given city name.
			*@param city the name of the city
			*@param province the name of the province or US state
			*@param country the name of the country
			*/
		virtual ASYNC setGeoLocation( const QString city, const QString province, const QString country ) = 0;
		
		/**Adjust one of the color settings.
			*@param colorName The name of the color to change (see one of the *.colors files, or colorscheme.cpp)
			*@param value The new color setting
			*/
		virtual ASYNC setColor( const QString colorName, const QString value ) = 0;
		
		/**Load a color scheme
			*@param name The name of the color scheme to be loaded
			*/
		virtual ASYNC loadColorScheme( const QString name ) = 0;
		
		/**Export an image of the current sky to a file on disk.
			*@param filename The filename for the exported image (the image type 
			*will be determined from the fileame extension; if this is not possible, 
			*it will save the image as a PNG)
			*@param width the width of the image
			*@param height the height of the image
			*/
		virtual ASYNC exportImage( const QString filename, int width, int height ) = 0;
		
		/**Print the current sky map.  Options to show the Print Dialog and to use Star Chart colors.
			*/
		virtual ASYNC printImage( bool usePrintDialog, bool useChartColors ) = 0;
		
		
		// Generic Device Functions
		/**Establish the device for an INDI-compatible device
		 *@param deviceName The INDI device name
		 *@param useLocal If true, starts the device in local mode. Otherwise, in server mode.
		*/
		virtual ASYNC startINDI (QString deviceName, bool useLocal) = 0;
		
		/**Shotdown a device
		 *@param deviceName The INDI device name
		*/
		virtual ASYNC shutdownINDI (QString deviceName) = 0;
		
		/**Turn the INDI device on/off
		 *@param deviceName The INDI device name
		 *@param turnOn If true, the device is switched on, otherwise it is switches off.
		*/
		virtual ASYNC switchINDI(QString deviceName, bool turnOn) = 0;
		
		/**Set INDI connection port
		 *@param deviceName The INDI device name
		 *@param port The connection port (e.g. /dev/ttyS0)
		*/
		virtual ASYNC setINDIPort(QString deviceName, QString port) = 0;
		
		/**Set INDI device action. This action is an element of a valid switch
		 * property in the device.
		 *@param deviceName The INDI device name
		 *@param action The generic action to invoke
		 */
		virtual ASYNC setINDIAction(QString deviceName, QString action) = 0;
		
		/** Wait for action to complete (state changed to OK or IDLE)
		 *@param deviceName The INDI device name
		 *@param action The action. The action can be any valid device property.
		 *               script will pause until the property status becomes OK.
		 */
		virtual ASYNC waitForINDIAction(QString deviceName, QString action) = 0;
		
		
		// Telescope Functions
		/**Set telescope target coordinates
		 *@param deviceName The INDI device name
		 *@param RA Target's right ascension in JNOW
		 *@param DEC Target's declination in JNOW
		 */
		virtual ASYNC setINDITargetCoord(QString deviceName, double RA, double DEC) = 0;
		
		/**Set telescope target
		 *@param deviceName The INDI device name
		 *@param objectName Object's name as found in KStars
		*/
		virtual ASYNC setINDITargetName(QString deviceName, QString objectName) = 0;
		
		/**Set telescope action
		 *@param deviceName The INDI device name
		 *@param action The specfic action to perform. Either SLEW, TRACK, SYNC, PARK, or ABORT.
		 */
		virtual ASYNC setINDIScopeAction(QString deviceName, QString action) = 0;
		
		/** Set INDI geographical location
		 *@param deviceName The INDI device name
		 *@param longitude Longitude expressed in double. E of N
		 *@param latitude Latitude expressed in double.
		 */
		virtual ASYNC setINDIGeoLocation(QString deviceName, double longitude, double latitude) = 0;
		
		/** Start INDI UTC date and time in ISO 8601 format
		 *@param deviceName The INDI device name
		 *@param UTCDateTime UTC date and time in ISO 8601 format.
		 */
		virtual ASYNC setINDIUTC(QString deviceName, QString UTCDateTime) = 0;
		
		
		// Focus Functions
		/** Set Focus Speed
		 *@param deviceName The INDI device name
		 *@param speed Focus speed: Halt, Fast, Medium, and Slow;
		 *             which numerical value corresponds to which
		 *             is left to the INDI driver.
		 *
		 *@todo Be more explicit about allowed action strings.
		*/
		virtual ASYNC setINDIFocusSpeed(QString deviceName, unsigned int speed) = 0;
		
		/** Set INDI focus timeout
		 *@param deviceName The INDI device name
		 *@param timeout Number of seconds to perform focusing.
		*/
		virtual ASYNC setINDIFocusTimeout(QString deviceName, int timeout) = 0;
		
		/** Start INDI focus operation in the selected direction
		 *@param deviceName The INDI device name
		 *@param focusDir Focus direction. If 0, focus in, if 1 focus out
		 */
		virtual ASYNC startINDIFocus(QString deviceName, int focusDir) = 0;
		
		// Filter Functions
		/** Sets the Filter position
		 *@param deviceName The INDI device name
		 *@param filter_num The filter position (0-20)
		 */
		virtual ASYNC setINDIFilterNum(QString deviceName, int filter_num) = 0;

		// Camera CCD Functions
		
		/** Sets the CCD camera frame type
		 *@param deviceName The INDI device name
		 *@param type The frame type can be either FRAME_LIGHT, FRAME_DARK,
		 *             FRAME_BIAS, or FRAME_FLAT
		 */
		virtual ASYNC setINDIFrameType(QString deviceName, QString type) = 0;
		
		/** Set CCD target temperature
		 *@param deviceName The INDI device name
		 *@param temp The target CCD temperature.
		 */
		virtual ASYNC setINDICCDTemp(QString deviceName, int temp) = 0;
		
		/** Start camera exposure
		 *@param deviceName The INDI device name
		 *@param timeout Number of seconds to perform exposure.
		*/
		virtual ASYNC startINDIExposure(QString deviceName, int timeout) = 0;
		
};

#endif
