#include <qnamespace.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <kglobal.h>
#include <klocale.h>

#include "kstars.h"
#include "infopanel.h"

class InfoPanel::pdata {
	public:
		pdata(const KLocale *loc) : locale(*loc) {};
		const KLocale &locale;
		QLabel *LT, *UT, *ST, *JD;
		QLabel *LTDate, *UTDate;
		QLabel *FocusObject, *FocusRA, *FocusDec, *FocusAlt, *FocusAz;
		QLabel *PlaceName, *Long, *Lat;
		QFrame *timef, *focusf, *placef;

		void showstuff(QWidget *thing, bool showit) {
			if (showit && thing->isHidden())
				thing->show();
			else if (! showit && !thing->isHidden())
				thing->hide();
		};
};


InfoPanel::InfoPanel(QWidget * parent, const char * name, const KLocale *loc, WFlags f ) :
	QFrame(parent, name, f)
{
	pd = new InfoPanel::pdata(loc);

	setFrameShape( QFrame::Panel );
	QPalette pal(palette());
	pal.setColor( QPalette::Normal, QColorGroup::Background, QColor( "Black" ) );
	pal.setColor( QPalette::Normal, QColorGroup::Foreground, QColor( "White" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Foreground, QColor( "White" ) );
	setPalette(pal);

	QVBoxLayout *iplay = new QVBoxLayout(this, 0, -1, "iplay" );

	//--------------------------------------------------------------
	//
	
	pd->timef = new QFrame(this, "timebar");
	iplay->addWidget(pd->timef);

	QHBoxLayout *hblo = new QHBoxLayout(pd->timef, 0, -1, "timelo");

	pd->LT = new QLabel(i18n( "Local Time", "LT:" ) + " 00:00:00    0000-00-00     ", pd->timef);
	// just so we make sure we have a consistent font, and to
	// gives a handle for changing the font in later, better code:
	QFont ipFont = pd->LT->font();
	pd->LT->setFont( ipFont );
	pd->LT->setFixedSize( pd->LT->sizeHint() );
	pd->LT->setPalette( pal );
	QToolTip::add( pd->LT, i18n( "Local Time" ) );

	hblo->addWidget( pd->LT);

//	hblo->addStretch(2);

	pd->UT = new QLabel( i18n( "Universal Time", "UT:" ) + " 00:00:00    0000-00-00         ", pd->timef);
	pd->UT->setFont( ipFont );
	pd->UT->setFixedSize( pd->UT->sizeHint() );
	pd->UT->setPalette( pal );
	QToolTip::add( pd->UT, i18n( "Universal Time" ) );

	hblo->addWidget( pd->UT);

	hblo->addStretch(2);

	pd->ST = new QLabel(i18n( "Sidereal Time", "ST:" ) + " 00:00:00   ", pd->timef);
	pd->ST->setFont( ipFont );
	pd->ST->setFixedSize( pd->ST->sizeHint() );
	pd->ST->setPalette( pal );
	QToolTip::add( pd->ST, i18n( "Sidereal Time" ) );

	hblo->addWidget( pd->ST);

//	hblo->addStretch(2);

	pd->JD = new QLabel("JD: 000000.00", pd->timef);
	pd->JD->setFont( ipFont );
	pd->JD->setPalette( pal );
	QToolTip::add( pd->JD, i18n( "Julian Day" ) );

	hblo->addWidget(pd->JD);

	//--------------------------------------------------------------
	//
	pd->focusf = new QFrame(this, "focusbar");
	iplay->addWidget(pd->focusf);

	hblo = new QHBoxLayout(pd->focusf, 0, -1, "focuslo");

	pd->FocusRA = new QLabel( i18n( "Right Ascension", "RA" ) + ": 00:00:00  ", pd->focusf );
	pd->FocusRA->setFont( ipFont );
	pd->FocusRA->setFixedSize( pd->FocusRA->sizeHint() );
	pd->FocusRA->setPalette( pal );
	QToolTip::add( pd->FocusRA, i18n( "Right Ascension of focus" ) );

	hblo->addWidget( pd->FocusRA);

	pd->FocusDec = new QLabel( i18n( "Declination", "Dec" ) + ": +00:00:00    ", pd->focusf );
	pd->FocusDec->setFont( ipFont );
	pd->FocusDec->setFixedSize( pd->FocusDec->sizeHint() );
	pd->FocusDec->setPalette( pal );
	QToolTip::add( pd->FocusDec, i18n( "Declination of focus" ) );

	hblo->addWidget( pd->FocusDec);

	pd->FocusAz = new QLabel( i18n( "Azimuth", "Az" ) + ": +000:00:00", pd->focusf );
	pd->FocusAz->setFont( ipFont );
	pd->FocusAz->setFixedSize( pd->FocusAz->sizeHint() );
	pd->FocusAz->setPalette( pal );
	QToolTip::add( pd->FocusAz, i18n( "Azimuth of focus" ) );

	hblo->addWidget( pd->FocusAz);

	pd->FocusAlt = new QLabel( i18n( "Altitude", "Alt" ) + ": +00:00:00    ", pd->focusf );
	pd->FocusAlt->setFont( ipFont );
	pd->FocusAlt->setFixedSize( pd->FocusAlt->sizeHint() );
	pd->FocusAlt->setPalette( pal );
	QToolTip::add( pd->FocusAlt, i18n( "Altitude of focus" ) );

	hblo->addWidget( pd->FocusAlt);
	
	pd->FocusObject = new QLabel( i18n( "Focused on: " ) + i18n( "nothing" ), pd->focusf );
	pd->FocusObject->setFont( ipFont );
	pd->FocusObject->setAlignment( AlignRight );
	pd->FocusObject->setPalette( pal );
	QToolTip::add( pd->FocusObject, i18n( "Name of object at focus" ) );

	hblo->addWidget(pd->FocusObject);

	//
	//--------------------------------------------------------------
	//
	pd->placef = new QFrame(this, "placebar");
	iplay->addWidget(pd->placef);

	hblo = new QHBoxLayout(pd->placef, 0, -1, "placelo");

	pd->PlaceName = new QLabel( i18n("nowhere"), pd->placef );
	pd->PlaceName->setFont( ipFont );
	pd->PlaceName->setPalette( pal );
	QToolTip::add( pd->PlaceName, i18n( "Name of Geographic Location" ) );

	pd->Long = new QLabel(i18n( "Longitude", "Long:" ) + " +000.00", pd->placef );
	pd->Long->setFont( ipFont );
	pd->Long->setPalette( pal );
	QToolTip::add( pd->Long, i18n( "Longitude of Geographic Location" ) );

	hblo->addWidget(pd->Long);

	hblo->addSpacing(20);

	pd->Lat = new QLabel(i18n( "Latitude", "Lat:" ) + " +000.00", pd->placef );
	pd->Lat->setFont( ipFont );
	pd->Lat->setPalette( pal );
	QToolTip::add( pd->Lat, i18n( "Latitude of Geographic Location" ) );

	hblo->addWidget(pd->Lat);

	hblo->addStretch(2);

	hblo->addWidget( pd->PlaceName);

}

void InfoPanel::timeChanged(QDateTime ut, QDateTime lt, QTime lst, long double julian) {

//not using locale.formatTime() because I couldn't find a way to control whether "pm" is displayed...
	// UTC
	pd->UT->setText( i18n( "Universal Time", "UT: " ) + ut.time().toString() + "   " + pd->locale.formatDate( ut.date(), true ) );

	// Local Time
	pd->LT->setText( i18n( "Local Time", "LT: " ) + lt.time().toString() + "   " + pd->locale.formatDate( lt.date(), true ) );

	// Julian
  //	pd->JD->setText( i18n( "Julian Day", "JD: " ) + QString("%1").arg( julian, 10, 'f', 2) );
	// Added locale() to localize display of decimal point number
	pd->JD->setText( i18n( "Julian Day", "JD: " ) + KGlobal::locale()->formatNumber( julian, 2 ));

	// LST
	QString dummy;
	QString STString = dummy.sprintf( "%02d:%02d:%02d", lst.hour(), lst.minute(), lst.second() );
	pd->ST->setText( i18n( "Sidereal Time", "ST: " ) + STString );
}

void InfoPanel::geoChanged(const GeoLocation *geo) {
	QString name = geo->translatedName() + ", ";

	if (!geo->province().isEmpty() ) {
		name += geo->translatedProvince() + ",  ";
	}

	name += geo->translatedCountry();

	pd->PlaceName->setText(name);

//	pd->Long->setText(i18n( "Longitude", "Long:" ) + " " + QString::number( geo->lng().Degrees(), 'f', 3 ) );
	pd->Long->setText(i18n( "Longitude", "Long:" ) + " " + 
		KGlobal::locale()->formatNumber( geo->lng().Degrees(),3));
//	pd->Lat->setText( i18n( "Latitude", "Lat:" ) + " " + QString::number( geo->lat().Degrees(), 'f', 3 ) );
	pd->Lat->setText( i18n( "Latitude", "Lat:" ) + " " + 
		KGlobal::locale()->formatNumber( geo->lat().Degrees(),3));
		
}

void InfoPanel::focusCoordChanged(const SkyPoint *p) {
	pd->FocusRA->setText( i18n( "Right Ascension", "RA" ) + ": " + p->ra().toHMSString());
	pd->FocusDec->setText( i18n( "Declination", "Dec" ) +  ": " + p->dec().toDMSString(true));
	pd->FocusAlt->setText( i18n( "Altitude", "Alt" ) + ": " + p->alt().toDMSString(true));
	pd->FocusAz->setText( i18n( "Azimuth", "Az" ) + ": " + p->az().toDMSString(true));
}

void InfoPanel::focusObjChanged(const QString &n) {
	pd->FocusObject->setText( i18n( "Focused on: " ) + n);
}

void InfoPanel::showInfoPanel(bool showit) {
	pd->showstuff(this, showit);
}

void InfoPanel::showTimeBar(bool showit) {
	pd->showstuff(pd->timef, showit);
}

void InfoPanel::showLocationBar(bool showit) {
	pd->showstuff(pd->placef, showit);
}

void InfoPanel::showFocusBar(bool showit) {
	pd->showstuff(pd->focusf, showit);
}

#include "infopanel.moc"

