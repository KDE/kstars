/***************************************************************************
                          elts.cpp  -  description
                             -------------------
    begin                : wed nov 17 08:05:11 CET 2002
    copyright            : (C) 2002-2003 by Pablo de Vicente
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
#include "elts.h"
#include "eltscanvas.h"
#include "skypoint.h"
#include "skyobject.h"
#include "geolocation.h"
#include "ksnumbers.h"

#include <qvariant.h>
#include <dmsbox.h>
#include <qdatetimeedit.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>
#include "ksutils.h"
#include "kstars.h"
#include "finddialog.h"
#include "locationdialog.h"
#include "geolocation.h"

elts::elts( QWidget* parent)  : 
	KDialogBase( KDialogBase::Plain, i18n( "Elevation vs. Time" ), Close, Close, parent ) 
{

	ks = (KStars*) parent;

	QFrame *page = plainPage();
	
	setMainWidget(page);
	QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

//	eltsTotalBox = new QGroupBox( this, "eltsBox" );
//	eltsTotalBox->setMinimumWidth( 580 );
//	eltsTotalBox->setMinimumHeight( 510 );
//	eltsTotalBox->setTitle( i18n( "Elevation vs. Time" ) );
//
//	eltsTotalBoxLayout = new QVBoxLayout( eltsTotalBox );
//	eltsTotalBoxLayout->setSpacing( 6 );
//	eltsTotalBoxLayout->setMargin( 11 );

	eltsView = new eltsCanvas( page );

	// each pixel 3 minutes x 30 arcmin 
	// Total size is X: 24x60 = 1440; 1440/3 = 480 + 2*40 (padding) = 560
	//            is Y: 360 + 2*40 (padding) = 440
	eltsView->setFixedSize( 560, 440 );

	ctlTabs = new QTabWidget( page, "DisplayTabs" );
//	ctlTabs->move( 10, 24 );
//	ctlTabs->setMinimumSize( 560, 110 );


	/** Tab for adding individual sources and clearing the plot */
	/** 1st Tab **/

	sourceTab = new QWidget( ctlTabs );
	sourceLayout = new QVBoxLayout( sourceTab, 0, 6 );

	nameLayout = new QHBoxLayout( 0, 2, 9 );
	coordLayout = new QHBoxLayout( 0, 2, 9 );

	nameLabel = new QLabel( sourceTab, "namebox" );
	nameLabel->setText( i18n( "Name:" ) );
	
	nameBox = new QLineEdit( sourceTab);
	
	browseButton = new QPushButton( sourceTab );
	browseButton->setText( i18n( "Browse" ) );
	
	nameLayout->addWidget( nameLabel );
	nameLayout->addWidget( nameBox );
	nameLayout->addSpacing( 12 );
	nameLayout->addWidget( browseButton );
	

	raLabel = new QLabel( sourceTab );
	raLabel->setText( i18n( "RA:" ) );
	
	raBox = new dmsBox( sourceTab , "rabox", FALSE);

	decLabel = new QLabel( sourceTab );
	decLabel->setText( i18n( "Dec:" ) );

	decBox = new dmsBox( sourceTab, "decbox" );

	epochLabel = new QLabel( sourceTab );
	epochLabel->setText( i18n( "Epoch:" ) );

	epochName = new QLineEdit( sourceTab, "epochname" );
	epochName->setMaximumSize( QSize( 60, 32767 ) );

	coordLayout->addWidget( raLabel );
	coordLayout->addWidget( raBox );
	coordLayout->addWidget( decLabel );
	coordLayout->addWidget( decBox );
	coordLayout->addWidget( epochLabel );
	coordLayout->addWidget( epochName );
	
	/** Buttons Layout */

	clearAddLayout = new QHBoxLayout( 0, 0, 6 );

	clearButton = new QPushButton( sourceTab );
	clearButton->setMinimumSize( QSize( 80, 0 ) );
	clearButton->setText( i18n( "Clear all" ) );

	QSpacerItem* spacer = new QSpacerItem( 10, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );

	addButton = new QPushButton( sourceTab );
	addButton->setMinimumSize( QSize( 80, 0 ) );
	addButton->setText( i18n( "Plot" ) );

	clearAddLayout->addItem( spacer );
	clearAddLayout->addWidget( clearButton );
	clearAddLayout->addWidget( addButton );

	/** Closing all layouts for Tab source*/

//	sourceLayout->addSpacing( 10 );
	sourceLayout->addLayout( nameLayout );
	sourceLayout->addLayout( coordLayout );
	sourceLayout->addLayout( clearAddLayout );

	ctlTabs->insertTab( sourceTab, i18n( "Source" ) );

	/** Tab for Date and Location */
	/** 2nd Tab */

	dateTab = new QWidget( ctlTabs );
	dateLocationLayout = new QVBoxLayout( dateTab, 0, 6, "dateLocationLayout"); 

	longLatLayout = new QHBoxLayout( 0, 2, 9, "longLatLayout"); 

	dateLabel = new QLabel( dateTab );
	dateLabel->setText( i18n( "Date:" ) );
	longLatLayout->addWidget( dateLabel );

	dateBox = new QDateEdit( dateTab, "datebox" );
	longLatLayout->addWidget( dateBox );

	latLabel = new QLabel( dateTab );
	latLabel->setText( i18n( "Lat.:" ) );
	longLatLayout->addWidget( latLabel );

	latBox = new dmsBox( dateTab, "latbox" );
	longLatLayout->addWidget( latBox );

	longLabel = new QLabel( dateTab );
	longLabel->setText( i18n( "Long.:" ) );
	longLatLayout->addWidget( longLabel );

	longBox = new dmsBox( dateTab, "longbox" );
	longLatLayout->addWidget( longBox );

	/* Layout for the button part */

	updateLayout = new QHBoxLayout( 0, 0, 6); 
	QSpacerItem* spacer_2 = new QSpacerItem( 10, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );

	cityButton = new QPushButton( dateTab );
	cityButton->setText( i18n( "Choose City..." ) );
	
	updateButton = new QPushButton( dateTab );
//	updateButton->setMinimumSize( QSize( 80, 0 ) );
	updateButton->setText( i18n( "Update" ) );
	
	updateLayout->addItem( spacer_2 );
	updateLayout->addWidget( cityButton );
	updateLayout->addWidget( updateButton );
	
	/** Closing layouts the date/location tab */

	dateLocationLayout->addLayout( longLatLayout );
	dateLocationLayout->addLayout( updateLayout );

	ctlTabs->insertTab( dateTab, i18n( "Date && Location" ) );

//	eltsTotalBoxLayout->addSpacing( 10 );
	topLayout->addWidget( eltsView );
	topLayout->addWidget( ctlTabs );

	showCurrentDate();
	//initGeo();
	showLongLat();
//	initVars();

	connect( browseButton, SIGNAL( clicked() ), this, SLOT( slotBrowseObject() ) );
	connect( cityButton, SIGNAL( clicked() ), this, SLOT( slotChooseCity() ) );
	connect( updateButton, SIGNAL( clicked() ), this, SLOT( slotUpdateDateLoc() ) );
	connect( clearButton, SIGNAL( clicked() ), this, SLOT( slotClear() ) );
	connect( addButton, SIGNAL( clicked() ), this, SLOT( slotAddSource() ) );
	connect( nameBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( raBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( decBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( longBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( latBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	
	pList.setAutoDelete(FALSE);
	deleteList.setAutoDelete(TRUE); //needed for skypoints which may be created in this class
}


/*  
 *  Destroys the object and frees any allocated resources
 */
elts::~elts()
{
    // no need to delete child widgets, Qt does it all for us
}

//QSize elts::sizeHint() const
//{
//	  return QSize(580,560);
//}

void elts::slotAddSource(void) {
	//First, attempt to parse the object name field
	if ( ! nameBox->text().isEmpty() ) {
		ObjectNameList &ObjNames = ks->data()->ObjNames;
		QString text = nameBox->text().lower();
		bool objFound(false);
	
		for( SkyObjectName *name = ObjNames.first( text ); name; name = ObjNames.next() ) {
			if ( name->text().lower() == text ) {
				//object found
				SkyObject *o = name->skyObject();
				processObject( o );
				objFound = true;
				break;
			}
		}
		if ( !objFound ) kdDebug() << "No object named " << nameBox->text() << " found." << endl;
	
	//Next, attempt to parse the coordinate fields
	} else if ( ! raBox->text().isEmpty() && ! decBox->text().isEmpty() ) {
		bool ok(false);
		dms ra, dec;
		ra = raBox->createDms(false, &ok);
		if ( ok ) decBox->createDms(true, &ok);
		
		if ( ok ) {
			SkyPoint *sp = new SkyPoint( ra, dec ); 
			double epoch0 = getEpoch( epochName->text() );
			double epoch  = QDateToEpoch( dateBox->date() );
			long double jd0 = epochToJd ( epoch0 );
			long double jd  = computeJdFromCalendar();
			sp->apparentCoord(jd0, jd);
			
			//If this point is not in list already, add it to list
			bool found(false);
			for ( SkyPoint *p = pList.first(); p; p = pList.next() ) {
				if ( sp->ra()->Degrees() == p->ra()->Degrees() && sp->dec()->Degrees() == p->dec()->Degrees() ) {
					found = true;
					break;
				}
			}
			if ( found ) kdDebug() << "This point is already displayed; I will not duplicate it." << endl;
			else {
				
				raBox->showInHours( sp->ra() );
				decBox->showInDegrees( sp->dec() );
				epochName->setText( QString().setNum( epoch ) );
				
				deleteList.append(sp);
				pList.append(sp);
			}
			kdDebug() << "Currently, there are " << pList.count() << " objects displayed." << endl;
		}
	}
	eltsView->repaint(false);
}

//Use find dialog to choose an object
void elts::slotBrowseObject(void) {
	FindDialog fd(ks);
	if ( fd.exec() == QDialog::Accepted ) {
		SkyObject *o = fd.currentItem()->objName()->skyObject();
		
		processObject( o );
		slotAddSource();
	} 
}

void elts::processObject( SkyObject *o ) {
	KSNumbers *num = new KSNumbers( computeJdFromCalendar() );
	
	//If the object is in the solar system, recompute its position for the given epochLabel
	if ( ks->data()->isSolarSystem( o ) ) {
		if ( o->type() == 2 && o->name() == "Moon" ) {
			((KSMoon*)o)->findPosition(num);
		} else if ( o->type() == 2 ) {
			((KSPlanet*)o)->findPosition(num);
		} else if ( o->type() == 9 ) {
			((KSComet*)o)->findPosition(num);
		} else if ( o->type() == 10 ) {
			((KSAsteroid*)o)->findPosition(num);
		} else {
			kdDebug() << "Error: I don't know what kind of body " << o->name() << " is." << endl;
		}
	}
	
	//precess coords to target epoch
	o->updateCoords( num );
	
	//If this point is not in list already, add it to list
	bool found(false);
	for ( SkyPoint *p = pList.first(); p; p = pList.next() ) {
		if ( o->ra()->Degrees() == p->ra()->Degrees() && o->dec()->Degrees() == p->dec()->Degrees() ) {
			found = true;
			break;
		}
	}
	if ( found ) kdDebug() << "This point is already displayed; I will not duplicate it." << endl;
	else {
		pList.append( (SkyPoint*)o );
		raBox->showInHours( o->ra() );
		decBox->showInDegrees( o->dec() );
		nameBox->setText( o->translatedName() );
		//Set epochName to epoch shown in date tab
		epochName->setText( QString().setNum( QDateToEpoch( dateBox->date() ) ) );
	}
	kdDebug() << "Currently, there are " << pList.count() << " objects displayed." << endl;
	
	delete num;
}

//move input focus to the next logical widget
void elts::slotAdvanceFocus(void) {
	if ( sender()->name() == QString( "namebox" ) ) addButton->setFocus();
	if ( sender()->name() == QString( "rabox" ) ) decBox->setFocus();
	if ( sender()->name() == QString( "decbox" ) ) addButton->setFocus();
	if ( sender()->name() == QString( "longbox" ) ) latBox->setFocus();
	if ( sender()->name() == QString( "latbox" ) ) updateButton->setFocus();
}

void elts::slotClear(void) {
	if ( pList.count() ) pList.clear();
	if ( deleteList.count() ) deleteList.clear();
	eltsView->repaint();
}

void elts::slotUpdateDateLoc(void) {
	eltsView->repaint();
}

void elts::slotChooseCity(void) {
	LocationDialog ld(ks);
	if ( ld.exec() == QDialog::Accepted ) {
		int ii = ld.getCityIndex();
		if ( ii >= 0 ) {
			GeoLocation *geo = ks->data()->geoList.at(ii);
			latBox->showInDegrees( geo->lat() );
			longBox->showInDegrees( geo->lng() );
		}
	}
}

void elts::showCurrentDate (void)
{
	QDateTime dt = QDateTime::currentDateTime();
	dateBox->setDate( dt.date() );
}

QDateTime elts::getQDate (void)
{
	QDateTime dt ( dateBox->date(),QTime(0,0,0) );
	return dt;
}

dms elts::getLongitude (void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

dms elts::getLatitude (void)
{
	dms latitude;
	latitude = latBox->createDms();
	return latitude;
}

void elts::initGeo(void)
{
	geoPlace = new GeoLocation( ks->geo() );
}

void elts::showLongLat(void)
{
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
}

long double elts::computeJdFromCalendar (void)
{
	long double julianDay;

	julianDay = KSUtils::UTtoJulian( getQDate() );

	return julianDay;
}

double elts::QDateToEpoch( const QDate &d )
{
	return double(d.year()) + double(d.dayOfYear())/double(d.daysInYear());
}

double elts::getEpoch (QString eName)
{
	//If Epoch field not a double, assume J2000
	bool ok(false);
	double epoch = eName.toDouble(&ok);
	
	if ( !ok ) {
		kdDebug() << "Invalid Epoch.  Assuming 2000.0." << endl;
		return 2000.0;
	}
	
	return epoch;
}

long double elts::epochToJd (double epoch)
{

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}

#include "elts.moc"
