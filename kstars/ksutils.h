#ifndef KSTARS_KSUTILS_H_
#define KSTARS_KSUTILS_H_

#include <qfile.h>
#include <qdatetime.h>

#include "dms.h"

class KSUtils {
	public:
		static long double UTtoJulian(const QDateTime &t);

		static QTime UTtoGST( const QDateTime &UT );

		/**convert Greenwich sidereal time to local sidereal time
			*@param GST the Greenwich Sidereal Time
			*@param longitude the current location's longitude
			*@returns QTime representing local sidereal time
			*/
		static QTime GSTtoLST( QTime GST, dms longitude );

		/**convert universal time to local sidereal time.
			*This is a convenience function: it calls UTtoGST, followed by GSTtoLST.
			*@param UT the Universal Time
			*@param longitude the current location's longitude
			*@returns QTime representing local sidereal time
			*/
		static QTime UTtoLST( QDateTime UT, dms longitude );

		/**Converts local sidereal time to universal time.
		 * @p UT = universal time
		 * @p longitude = east longitude
		 * @returns QTime representing universal time
		 */
		static QTime LSTtoUT( QDateTime LST, dms longitude);

		/**Convenience function to compute the Greenwich sidereal 
		 * time at 0h of universal time. This function is called
		 * by other functions.
		 *
		 * @p DT = date and time
		 * @returns sidereal time in hours.
		 */
		static double GSTat0hUT( QDateTime DT );

		/**Computes date and time from the julian day.
		 * @p jd = julian day
		 * @returns QDateTime representing date and time
		 */
		static QDateTime JDtoDateTime(long double jd);

		static void nutate( long double jd, double &dEcLong, double &dObliq );

		static dms findObliquity( long double jd );

	/**
		*Attempt to open the data file named filename, using the QFile object "file".
		*First look in the standard KDE directories, then look in a local "data"
		*subdirectory.  If the data file cannot be found or opened, display a warning
		*messagebox.
		*@short Open a data file.
		*@param &file The QFile object to be opened
		*@param filename The name of the data file.
		*@returns bool Returns true if data file was opened successfully.
		*/

		static bool openDataFile( QFile &file, QString filename );

		/* Extracts the Julian Day at 0h UT.
		* @param j The julian day
		* @returns julian day at 0h UT
		* */
		static long double JdAtZeroUT (long double j);

	private:
		//
		// So you can't instantiate the class.
		//
		KSUtils();
};

#endif
