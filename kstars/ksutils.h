/***************************************************************************
                          kstars.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jan 7 2002
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

/**@class KSUtils
	*@short KStars utility functions
	*@author Mark Hollomon
	*@version 1.0
	*Static functions for various purposes.
	*The openDataFile() function searches the standard KDE directories 
	*for the data filename given as an argument.
	*(it is impossible to instantiate a KSUtils object; just use the static functions).
	*/

#ifndef KSTARS_KSUTILS_H_
#define KSTARS_KSUTILS_H_

class QFile;

class KSUtils {
	public:
	/**Attempt to open the data file named filename, using the QFile object "file".
		*First look in the standard KDE directories, then look in a local "data"
		*subdirectory.  If the data file cannot be found or opened, display a warning
		*messagebox.
		*@short Open a data file.
		*@param &file The QFile object to be opened
		*@param filename The name of the data file.
		*@returns bool Returns true if data file was opened successfully.
		*@returns a reference to the opened file.
		*/
		static bool openDataFile( QFile &file, const QString &filename );

	/** Lagrange interpolation using a maximum number of 10 points.
	 	*@param x[] double array with x values
	 	*@param v[] double array with y values
		*@param n number of points to use for interpolation
		*@param xval value for which we are looking for the y value.
	 	*/
		static long double lagrangeInterpolation(const long double x[], const long double v[], int n, long double xval);

	private:
	/**Constructor.  This class is just a collection of static functions, so 
		*we have made the constructor private (so it is not possible to  
		*instantiate a KSUtils object).
		*/
		KSUtils();
};

#endif
