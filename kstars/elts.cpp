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
#include "dms.h"
#include "plotwidget.h"
#include "skypoint.h"
#include "skyobject.h"
#include "geolocation.h"
#include "ksnumbers.h"

#include <klineedit.h>
#include <klistbox.h>
#include <qvariant.h>
#include "dmsbox.h"
#include <qdatetimeedit.h>
#include <qgroupbox.h>
#include <qlabel.h>
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

elts::elts( QWidget* parent)  :
	KDialogBase( KDialogBase::Plain, i18n( "Altitude vs. Time" ), Close, Close, parent )
{

	ks = (KStars*) parent;

	QFrame *page = plainPage();

	setMainWidget(page);
	QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

	View = new PlotWidget( 0, 24, -90, 90, page );
	View->setFixedSize( 500, 400 );
	View->setXAxisType( PlotWidget::TIME );
	View->setYAxisType( PlotWidget::ANGLE );
	View->updateTickmarks();

	ctlTabs = new QTabWidget( page, "DisplayTabs" );


	/** Tab for adding individual sources and clearing the plot */
	/** 1st Tab **/

	sourceTab        = new QWidget( ctlTabs );
	sourceLayout     = new QHBoxLayout( sourceTab, 0, 6 );
	sourceLeftLayout = new QVBoxLayout( 0, 2, 9 );
	sourceMidLayout  = new QVBoxLayout( 0, 2, 9 );
	nameLayout     = new QHBoxLayout( 0, 2, 9 );
	coordLayout    = new QHBoxLayout( 0, 2, 9 );
	clearAddLayout = new QHBoxLayout( 0, 2, 9 );

	//labels for sourceLeftLayout
	nameLabel = new QLabel( sourceTab, "namebox" );
	nameLabel->setText( i18n( "Name:" ) );
	raLabel = new QLabel( sourceTab );
	raLabel->setText( i18n( "RA:" ) );
	epochLabel = new QLabel( sourceTab );
	epochLabel->setText( i18n( "Epoch:" ) );

	sourceLeftLayout->addWidget( nameLabel );
	sourceLeftLayout->addWidget( raLabel );
	sourceLeftLayout->addWidget( epochLabel );

	//widgets for nameLayout
	nameBox = new KLineEdit( sourceTab );
	browseButton = new QPushButton( sourceTab );
	browseButton->setText( i18n( "Browse" ) );

	nameLayout->addWidget( nameBox );
//	nameLayout->addSpacing( 12 );
	nameLayout->addWidget( browseButton );

	//widgets for coordLayout
	raBox = new dmsBox( sourceTab , "rabox", FALSE);
	decLabel = new QLabel( sourceTab );
	decLabel->setText( i18n( "Dec:" ) );
	decBox = new dmsBox( sourceTab, "decbox" );

	coordLayout->addWidget( raBox );
	coordLayout->addSpacing( 12 );
	coordLayout->addWidget( decLabel );
	coordLayout->addWidget( decBox );

	//widgets for clearAddLayout
	epochName = new KLineEdit( sourceTab, "epochname" );
	epochName->setMaximumSize( QSize( 80, 32767 ) );
	clearFieldsButton = new QPushButton( sourceTab );
	clearFieldsButton->setMinimumSize( QSize( 80, 0 ) );
	clearFieldsButton->setText( i18n( "Clear Fields" ) );
	addButton = new QPushButton( sourceTab );
	addButton->setMinimumSize( QSize( 60, 0 ) );
	addButton->setText( i18n( "Plot" ) );
	clearButton = new QPushButton( sourceTab );
	clearButton->setMinimumSize( QSize( 80, 0 ) );
	clearButton->setText( i18n( "Clear List" ) );

	clearAddLayout->addWidget( epochName );
	clearAddLayout->addItem( new QSpacerItem( 6, 0, QSizePolicy::Expanding, QSizePolicy::Minimum ) );
	clearAddLayout->addWidget( clearFieldsButton );
	clearAddLayout->addWidget( addButton );
	clearAddLayout->addWidget( clearButton );

	PlotList = new KListBox( sourceTab );
	PlotList->setVScrollBarMode( QScrollView::AlwaysOn );

	//populate sourceMidLayout
	sourceMidLayout->addLayout( nameLayout );
	sourceMidLayout->addLayout( coordLayout );
	sourceMidLayout->addLayout( clearAddLayout );

	//populate sourceLayout
	sourceLayout->addLayout( sourceLeftLayout );
	sourceLayout->addLayout( sourceMidLayout );
	sourceLayout->addWidget( PlotList );

	ctlTabs->insertTab( sourceTab, i18n( "Sources" ) );

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
	topLayout->addWidget( View );
	topLayout->addWidget( ctlTabs );

	showCurrentDate();
	initGeo();
	showLongLat();
//	initVars();

	connect( browseButton, SIGNAL( clicked() ), this, SLOT( slotBrowseObject() ) );
	connect( cityButton,   SIGNAL( clicked() ), this, SLOT( slotChooseCity() ) );
	connect( updateButton, SIGNAL( clicked() ), this, SLOT( slotUpdateDateLoc() ) );
	connect( clearButton, SIGNAL( clicked() ), this, SLOT( slotClear() ) );
	connect( addButton,   SIGNAL( clicked() ), this, SLOT( slotAddSource() ) );
	connect( nameBox, SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
	connect( raBox,   SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
	connect( decBox,  SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
	//CLEAR_FIELDS
//	connect( nameBox, SIGNAL( gotFocus() ), this, SLOT( slotClearBoxes() ) );
//	connect( raBox,   SIGNAL( gotFocus() ), this, SLOT( slotClearBoxes() ) );
//	connect( decBox,  SIGNAL( gotFocus() ), this, SLOT( slotClearBoxes() ) );
	connect( clearFieldsButton, SIGNAL( clicked() ), this, SLOT( slotClearBoxes() ) );
	connect( longBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( latBox,  SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
//	connect( PlotList, SIGNAL( highlighted(int) ), eltsView, SLOT( update() ) );
	connect( PlotList, SIGNAL( highlighted(int) ), this, SLOT( slotHighlight() ) );

	pList.setAutoDelete(FALSE);
	deleteList.setAutoDelete(TRUE); //needed for skypoints which may be created in this class

	//ther edit boxes should not pass on the return key!
	nameBox->setTrapReturnKey( true );
	raBox->setTrapReturnKey( true );
	decBox->setTrapReturnKey( true );

	//CLEAR_FIELDS
//	dirtyFlag = false;
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
	bool objFound( false );

	//First, attempt to find the object name in the list of known objects
	if ( ! nameBox->text().isEmpty() ) {
		ObjectNameList &ObjNames = ks->data()->ObjNames;
		QString text = nameBox->text().lower();

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
	}

	//Next, if the name, RA, and Dec fields are all filled, construct a new skyobject
	//with these parameters
	if ( !objFound && ! nameBox->text().isEmpty() && ! raBox->text().isEmpty() && ! decBox->text().isEmpty() ) {
		bool ok( true );
		dms newRA( 0.0 ), newDec( 0.0 );
		newRA = raBox->createDms( false, &ok );
		if ( ok ) newDec = decBox->createDms( true, &ok );

		//make sure the coords do not already exist from another object
		bool found(false);
		for ( SkyPoint *p = pList.first(); p; p = pList.next() ) {
			//within an arcsecond?
			if ( fabs( newRA.Degrees() - p->ra()->Degrees() ) < 0.0003 && fabs( newDec.Degrees() - p->dec()->Degrees() ) < 0.0003 ) {
				found = true;
				break;
			}
		}
		if ( found ) {
			kdDebug() << "This point is already displayed; I will not duplicate it." << endl;
			ok = false;
		}

		if ( ok ) {
			SkyObject *obj = new SkyObject( 8, newRA, newDec, 1.0, nameBox->text() );
			deleteList.append( obj ); //this object will be deleted when window is destroyed
			processObject( obj );
		}

	//If the Ra and Dec boxes are filled, but the name field is empty,
	//move input focus to nameBox`
	} else if ( nameBox->text().isEmpty() && ! raBox->text().isEmpty() && ! decBox->text().isEmpty() ) {
		nameBox->QWidget::setFocus();

	//nameBox is empty, and one of the ra or dec fields is empty.  Move input focus to empty coord box
	} else if (  raBox->text().isEmpty() ) {
		raBox->QWidget::setFocus();
	} else if ( decBox->text().isEmpty() ) {
		decBox->QWidget::setFocus();
	}

	View->repaint(false);
}

//Use find dialog to choose an object
void elts::slotBrowseObject(void) {
	FindDialog fd(ks);
	if ( fd.exec() == QDialog::Accepted ) {
		SkyObject *o = fd.currentItem()->objName()->skyObject();
		processObject( o );
	}

	View->repaint();
}

void elts::processObject( SkyObject *o, bool forceAdd ) {
	//We need earth for findPosition.  Store KSNumbers for simulation date/time
	//so we can restore Earth position later.
	KSNumbers *num = new KSNumbers( computeJdFromCalendar() );
	KSNumbers *oldNum = 0;
	KSPlanet *Earth = ks->data()->earth();

	//If the object is in the solar system, recompute its position for the given epochLabel
	if ( ks->data()->isSolarSystem( o ) ) {
		oldNum = new KSNumbers( ks->clock()->JD() );
		Earth->findPosition( num );

		if ( o->type() == 2 && o->name() == "Moon" ) {
			((KSMoon*)o)->findPosition(num, Earth);
		} else if ( o->type() == 2 ) {
			((KSPlanet*)o)->findPosition(num, Earth);
		} else if ( o->type() == 9 ) {
			((KSComet*)o)->findPosition(num, Earth);
		} else if ( o->type() == 10 ) {
			kdDebug() << "Asteroid findPosition().  Name = " << o->name() << endl;
			((KSAsteroid*)o)->findPosition(num, Earth);
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
	if ( found && !forceAdd ) kdDebug() << "This point is already displayed; I will not duplicate it." << endl;
	else {
		pList.append( (SkyPoint*)o );

		//make sure existing curves are thin and red
		for ( int i=0; i<View->objectCount(); ++i ) {
			PlotObject *obj = View->object( i );
			if ( obj->size() == 2 ) {
				obj->setColor( "red" );
				obj->setSize( 1 );
			}
		}

		//add new curve with width=2, and color=white
		PlotObject *po = new PlotObject( "", "white", PlotObject::CURVE, 2, PlotObject::SOLID );
		for ( double h=0.0; h<=24.0; h+=0.5 ) {
			po->addPoint( new DPoint( h, findAltitude( o, h ) ) );
		}
		View->addObject( po );

		PlotList->insertItem( o->name() );
		PlotList->setCurrentItem( PlotList->count() - 1 );
		raBox->showInHours(o->ra() );
		decBox->showInDegrees(o->dec() );
		nameBox->setText(o->name() );

		//CLEAR_FIELDS
//		dirtyFlag = true;

		//Set epochName to epoch shown in date tab
		epochName->setText( QString().setNum( QDateToEpoch( dateBox->date() ) ) );
	}
	kdDebug() << "Currently, there are " << View->objectCount() << " objects displayed." << endl;

	//restore original Earth position
	if ( ks->data()->isSolarSystem( o ) ) {
		Earth->findPosition( oldNum );
		delete oldNum;
	}

	delete num;
}

double elts::findAltitude( SkyPoint *p, double hour ) {
	int h = int(hour);
	int m = int(60.*(hour - h));
	int s = int(60.*(60.*(hour - h) - m));
	QDateTime ut( getQDate().date(), QTime( h,m,s ) );
	dms lat = getLatitude();
	dms lgt = getLongitude();
	dms LST = KSUtils::UTtoLST( ut , &lgt );
	p->EquatorialToHorizontal( &LST, &lat );

	return p->alt()->Degrees();
}

void elts::slotHighlight(void) {
	int iPlotList = PlotList->currentItem();

	//highlight the curve of the selected object
	for ( int i=0; i<View->objectCount(); ++i ) {
		PlotObject *obj = View->object( i );

		if ( i == iPlotList ) {
			obj->setSize( 2 );
			obj->setColor( "white" );
		} else if ( obj->size() == 2 ) {
			obj->setSize( 1 );
			obj->setColor( "red" );
		}
	}

	View->update();

	int index = 0;
	for ( SkyPoint *p = pList.first(); p; p = pList.next() ) {
		if ( index == iPlotList ) {
			raBox->showInHours(p->ra() );
			decBox->showInDegrees(p->dec() );
			nameBox->setText(PlotList->currentText() );
		}
		++index;
	}
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
	PlotList->clear();
	nameBox->clear();
	raBox->clear();
	decBox->clear();
	View->clearObjectList();
	View->repaint();

	//CLEAR_FIELDS
//	dirtyFlag = false;
}

void elts::slotClearBoxes(void) {
	//clear the name, ra, and dec fields
	// This still does not work. I will have a look later.

	//CLEAR_FIELDS Two next lines
//	if ( dirtyFlag ) {
//		dirtyFlag = false;
		nameBox->clear();
		raBox->clear() ;
		decBox->clear();
		epochName->setText( QString().setNum( QDateToEpoch( dateBox->date() ) ) );

	//CLER_FIELDS
//	}
}

void elts::slotUpdateDateLoc(void) {
	//reprocess objects to update their coordinates.
	//Again find each object in the list of known objects, and update
	//coords if the object is a solar system body
	KSNumbers *num = new KSNumbers( computeJdFromCalendar() );
	KSPlanet *Earth = ks->data()->earth();

	for ( unsigned int i = 0; i < PlotList->count(); ++i ) {
		QString oName = PlotList->text( i ).lower();
		ObjectNameList &ObjNames = ks->data()->ObjNames;
		bool objFound(false);

		kdDebug() << i << ": " << oName << endl;

		for( SkyObjectName *name = ObjNames.first( oName ); name; name = ObjNames.next() ) {
			if ( name->text().lower() == oName ) {
				//object found
				SkyObject *o = name->skyObject();

				//If the object is in the solar system, recompute its position for the given date
				if ( ks->data()->isSolarSystem( o ) ) {
					Earth->findPosition( num );

					if ( o->type() == 2 && o->name() == "Moon" ) {
						((KSMoon*)o)->findPosition(num, Earth);
					} else if ( o->type() == 2 ) {
						((KSPlanet*)o)->findPosition(num, Earth);
					} else if ( o->type() == 9 ) {
						((KSComet*)o)->findPosition(num, Earth);
					} else if ( o->type() == 10 ) {
						((KSAsteroid*)o)->findPosition(num, Earth);
					} else {
						kdDebug() << "Error: I don't know what kind of body " << o->name() << " is." << endl;
					}
				}

				//precess coords to target epoch
				o->updateCoords( num );

				//update pList entry
				pList.replace( i, (SkyPoint*)o );

				PlotObject *po = new PlotObject( "", "white", PlotObject::CURVE, 1, PlotObject::SOLID );
				for ( double h=0.0; h<=24.0; h+=0.5 ) {
					po->addPoint( new DPoint( h, findAltitude( o, h ) ) );
				}
				View->replaceObject( i, po );

				objFound = true;
				break;
			}
		}
	}

	View->repaint();
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

double elts::getTZ( void ) {
	return geoPlace->TZ();
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

	julianDay = KSUtils::UTtoJD( getQDate() );

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
