#ifndef KSTARS_INFOPANEL_H_
#define KSTARS_INFOPANEL_H_

#include <qframe.h>
#include <qlabel.h>
#include <qdatetime.h>

#include <klocale.h>

#include "geolocation.h"
#include "skypoint.h"

class InfoPanel : public QFrame {

	Q_OBJECT

	public:

		InfoPanel(QWidget * parent, const char * name, const KLocale *loc, WFlags f = 0 );

	public slots:

		void timeChanged(QDateTime ut, QDateTime lt, QTime lst, long double julian);
		void geoChanged(const GeoLocation *geo);
		void focusCoordChanged(const SkyPoint *p);
		void focusObjChanged(const QString &n);
		void showInfoPanel(bool showit);
		void showTimeBar(bool showit);
		void showFocusBar(bool showit);
		void showLocationBar(bool showit);

	private:
		class pdata;

		pdata *pd;
};

#endif
