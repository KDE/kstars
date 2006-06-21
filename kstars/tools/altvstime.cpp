/***************************************************************************
                          altvstime.cpp  -  description
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

#include <qlayout.h>
#include <qpainter.h>
#include <klocale.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>
#include <kdialogbase.h>

#include "altvstime.h"
#include "altvstimeui.h"
#include "dms.h"
#include "dmsbox.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skypoint.h"
#include "skyobject.h"
#include "skyobjectname.h"
#include "ksnumbers.h"
#include "objectnamelist.h"
#include "simclock.h"
#include "finddialog.h"
#include "locationdialog.h"

#include "kstarsdatetime.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

AltVsTime::AltVsTime( QWidget* parent)  :
	KDialogBase( KDialogBase::Plain, i18n( "Altitude vs. Time" ), Close, Close, parent )
{
	ks = (KStars*) parent;

	QFrame *page = plainPage();

	setMainWidget(page);
	topLayout = new QVBoxLayout( page, 0, spacingHint() );

	View = new AVTPlotWidget( -12.0, 12.0, -90.0, 90.0, page );
	View->setMinimumSize( 400, 400 );
	View->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
	View->setXAxisType( KStarsPlotWidget::TIME );
	View->setYAxisType( KStarsPlotWidget::ANGLE );
	View->setShowGrid( false );
	View->setXAxisLabel( i18n( "Local Time" ) );
	View->setXAxisLabel2( i18n( "Local Sidereal Time" ) );
	View->setYAxisLabel( i18n( "the angle of an object above (or below) the horizon", "Altitude" ) );

	avtUI = new AltVsTimeUI( page );
	avtUI->raBox->setDegType( false );
	avtUI->decBox->setDegType( true );

	//POST-3.2
	//Doesn't make sense to manually adjust long/lat unless we can modify TZ also
	avtUI->longBox->setReadOnly( true );
	avtUI->latBox->setReadOnly( true );

	topLayout->addWidget( View );
	topLayout->addWidget( avtUI );

	geo = ks->geo();

	DayOffset = 0;
	showCurrentDate();
	if ( getDate().time().hour() > 12 ) DayOffset = 1;

	avtUI->longBox->show( geo->lng() );
	avtUI->latBox->show( geo->lat() );

	computeSunRiseSetTimes();

	setLSTLimits();
	View->updateTickmarks();

	connect( avtUI->browseButton, SIGNAL( clicked() ), this, SLOT( slotBrowseObject() ) );
	connect( avtUI->cityButton,   SIGNAL( clicked() ), this, SLOT( slotChooseCity() ) );
	connect( avtUI->updateButton, SIGNAL( clicked() ), this, SLOT( slotUpdateDateLoc() ) );
	connect( avtUI->clearButton, SIGNAL( clicked() ), this, SLOT( slotClear() ) );
	connect( avtUI->addButton,   SIGNAL( clicked() ), this, SLOT( slotAddSource() ) );
	connect( avtUI->nameBox, SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
	connect( avtUI->raBox,   SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
	connect( avtUI->decBox,  SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
	connect( avtUI->clearFieldsButton, SIGNAL( clicked() ), this, SLOT( slotClearBoxes() ) );
	connect( avtUI->longBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( avtUI->latBox,  SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
	connect( avtUI->PlotList, SIGNAL( highlighted(int) ), this, SLOT( slotHighlight() ) );

	pList.setAutoDelete(FALSE);
	deleteList.setAutoDelete(TRUE); //needed for skypoints which may be created in this class

	//ther edit boxes should not pass on the return key!
	avtUI->nameBox->setTrapReturnKey( true );
	avtUI->raBox->setTrapReturnKey( true );
	avtUI->decBox->setTrapReturnKey( true );

	setMouseTracking( true );
}

AltVsTime::~AltVsTime()
{
    // no need to delete child widgets, Qt does it all for us
}

void AltVsTime::slotAddSource(void) {
	bool objFound( false );

	//First, attempt to find the object name in the list of known objects
	if ( ! avtUI->nameBox->text().isEmpty() ) {
		ObjectNameList &ObjNames = ks->data()->ObjNames;
		QString text = avtUI->nameBox->text().lower();

		for( SkyObjectName *name = ObjNames.first( text ); name; name = ObjNames.next() ) {
			if ( name->text().lower() == text ) {
				//object found
				SkyObject *o = name->skyObject();
				processObject( o );

				objFound = true;
				break;
			}
		}

		if ( !objFound ) kdDebug() << "No object named " << avtUI->nameBox->text() << " found." << endl;
	}

	//Next, if the name, RA, and Dec fields are all filled, construct a new skyobject
	//with these parameters
	if ( !objFound && ! avtUI->nameBox->text().isEmpty() && ! avtUI->raBox->text().isEmpty() && ! avtUI->decBox->text().isEmpty() ) {
		bool ok( true );
		dms newRA( 0.0 ), newDec( 0.0 );
		newRA = avtUI->raBox->createDms( false, &ok );
		if ( ok ) newDec = avtUI->decBox->createDms( true, &ok );

		//If the epochName is blank (or any non-double), we assume J2000
		//Otherwise, precess to J2000.
		KStarsDateTime dt;
		dt.setFromEpoch( getEpoch( avtUI->epochName->text() ) );
		long double jd = dt.djd();
		if ( jd != J2000 ) {
			SkyPoint ptest( newRA, newDec );
			ptest.precessFromAnyEpoch( jd, J2000 );
			newRA.setH( ptest.ra()->Hours() );
			newDec.setD( ptest.dec()->Degrees() );
		}

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
			SkyObject *obj = new SkyObject( 8, newRA, newDec, 1.0, avtUI->nameBox->text() );
			deleteList.append( obj ); //this object will be deleted when window is destroyed
			processObject( obj );
		}

	//If the Ra and Dec boxes are filled, but the name field is empty,
	//move input focus to nameBox`
	} else if ( avtUI->nameBox->text().isEmpty() && ! avtUI->raBox->text().isEmpty() && ! avtUI->decBox->text().isEmpty() ) {
		avtUI->nameBox->QWidget::setFocus();

	//nameBox is empty, and one of the ra or dec fields is empty.  Move input focus to empty coord box
	} else if ( avtUI->raBox->text().isEmpty() ) {
		avtUI->raBox->QWidget::setFocus();
	} else if ( avtUI->decBox->text().isEmpty() ) {
		avtUI->decBox->QWidget::setFocus();
	}

	View->repaint(false);
}

//Use find dialog to choose an object
void AltVsTime::slotBrowseObject(void) {
	FindDialog fd(ks);
	if ( fd.exec() == QDialog::Accepted ) {
		SkyObject *o = fd.currentItem()->objName()->skyObject();
		processObject( o );
	}

	View->repaint();
}

void AltVsTime::processObject( SkyObject *o, bool forceAdd ) {
	KSNumbers *num = new KSNumbers( getDate().djd() );
	KSNumbers *oldNum = 0;

	//If the object is in the solar system, recompute its position for the given epochLabel
	if ( o && o->isSolarSystem() ) {
		oldNum = new KSNumbers( ks->data()->ut().djd() );
		o->updateCoords( num, true, geo->lat(), ks->LST() );
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
		for ( int i=0; i < View->objectCount(); ++i ) {
			KPlotObject *obj = View->object( i );
			if ( obj->size() == 2 ) {
				obj->setColor( "red" );
				obj->setSize( 1 );
			}
		}

		//add new curve with width=2, and color=white
		KPlotObject *po = new KPlotObject( "", "white", KPlotObject::CURVE, 2, KPlotObject::SOLID );
		for ( double h=-12.0; h<=12.0; h+=0.5 ) {
			po->addPoint( new DPoint( h, findAltitude( o, h ) ) );
		}
		View->addObject( po );

		avtUI->PlotList->insertItem( o->translatedName() );
		avtUI->PlotList->setCurrentItem( avtUI->PlotList->count() - 1 );
		avtUI->raBox->showInHours(o->ra() );
		avtUI->decBox->showInDegrees(o->dec() );
		avtUI->nameBox->setText(o->translatedName() );

		//Set epochName to epoch shown in date tab
		avtUI->epochName->setText( QString().setNum( getDate().epoch() ) );
	}
	kdDebug() << "Currently, there are " << View->objectCount() << " objects displayed." << endl;

	//restore original position
	if ( o->isSolarSystem() ) {
		o->updateCoords( oldNum, true, ks->geo()->lat(), ks->LST() );
                delete oldNum;
	}
        o->EquatorialToHorizontal( ks->LST(), ks->geo()->lat() );
	delete num;
}

double AltVsTime::findAltitude( SkyPoint *p, double hour ) {
	hour += 24.0*(double)DayOffset;

	//getDate converts the user-entered local time to UT
	KStarsDateTime ut = getDate().addSecs( hour*3600.0 );

	dms LST = geo->GSTtoLST( ut.gst() );
	p->EquatorialToHorizontal( &LST, geo->lat() );
	return p->alt()->Degrees();
}

void AltVsTime::slotHighlight(void) {
	int iPlotList = avtUI->PlotList->currentItem();

	//highlight the curve of the selected object
	for ( int i=0; i<View->objectCount(); ++i ) {
		KPlotObject *obj = View->object( i );

		if ( i == iPlotList ) {
			obj->setSize( 2 );
			obj->setColor( "white" );
		} else {
			obj->setSize( 1 );
			obj->setColor( "red" );
		}
	}

	View->update();

	int index = 0;
	for ( SkyPoint *p = pList.first(); p; p = pList.next() ) {
		if ( index == iPlotList ) {
			avtUI->raBox->showInHours(p->ra() );
			avtUI->decBox->showInDegrees(p->dec() );
			avtUI->nameBox->setText(avtUI->PlotList->currentText() );
		}
		++index;
	}
}

//move input focus to the next logical widget
void AltVsTime::slotAdvanceFocus(void) {
	if ( sender()->name() == QString( "nameBox" ) ) avtUI->addButton->setFocus();
	if ( sender()->name() == QString( "raBox" ) ) avtUI->decBox->setFocus();
	if ( sender()->name() == QString( "decbox" ) ) avtUI->addButton->setFocus();
	if ( sender()->name() == QString( "longBox" ) ) avtUI->latBox->setFocus();
	if ( sender()->name() == QString( "latBox" ) ) avtUI->updateButton->setFocus();
}

void AltVsTime::slotClear(void) {
	if ( pList.count() ) pList.clear();
	if ( deleteList.count() ) deleteList.clear();
	avtUI->PlotList->clear();
	avtUI->nameBox->clear();
	avtUI->raBox->clear();
	avtUI->decBox->clear();
        avtUI->epochName->clear();
	View->clearObjectList();
	View->repaint();
}

void AltVsTime::slotClearBoxes(void) {
	avtUI->nameBox->clear();
	avtUI->raBox->clear() ;
	avtUI->decBox->clear();
	avtUI->epochName->clear();
}

void AltVsTime::computeSunRiseSetTimes() {
	//Determine the time of sunset and sunrise for the desired date and location
	//expressed as doubles, the fraction of a full day.
	KStarsDateTime today = getDate();

	SkyObject *oSun = (SkyObject*) ks->data()->PCat->planetSun();
	double sunRise = -1.0 * oSun->riseSetTime( today.djd() + 1.0, geo, true ).secsTo(QTime()) / 86400.0;
	double sunSet = -1.0 * oSun->riseSetTime( today.djd(), geo, false ).secsTo(QTime()) / 86400.0;

	//check to see if Sun is circumpolar
	//requires temporary repositioning of Sun to target date
	KSNumbers *num = new KSNumbers( today.djd() );
	KSNumbers *oldNum = new KSNumbers( ks->data()->ut().djd() );
	dms LST = geo->GSTtoLST( getDate().gst() );
	oSun->updateCoords( num, true, geo->lat(), &LST );
	if ( oSun->checkCircumpolar( geo->lat() ) ) {
		if ( oSun->alt()->Degrees() > 0.0 ) {
			//Circumpolar, signal it this way:
			sunRise = 0.0;
			sunSet = 1.0;
		} else {
			//never rises, signal it this way:
			sunRise = 0.0;
			sunSet = -1.0;
		}
	}

	//Notify the View about new sun rise/set times:
	View->setSunRiseSetTimes( sunRise, sunSet );

	//Restore Sun coordinates:
	oSun->updateCoords( oldNum, true, ks->geo()->lat(), ks->LST() );
        oSun->EquatorialToHorizontal( ks->LST(), ks->geo()->lat() );

	delete num;
	delete oldNum;
}

void AltVsTime::slotUpdateDateLoc(void) {
	KStarsDateTime today = getDate();
	KSNumbers *num = new KSNumbers( today.djd() );
	KSNumbers *oldNum = 0;
	dms LST = geo->GSTtoLST( today.gst() );

	//First determine time of sunset and sunrise
	computeSunRiseSetTimes();

	for ( unsigned int i = 0; i < avtUI->PlotList->count(); ++i ) {
		QString oName = avtUI->PlotList->text( i ).lower();
		ObjectNameList &ObjNames = ks->data()->ObjNames;
		bool objFound(false);

		for( SkyObjectName *name = ObjNames.first( oName ); name; name = ObjNames.next() ) {
			if ( name->text().lower() == oName ) {
				//object found
				SkyObject *o = name->skyObject();

				//If the object is in the solar system, recompute its position for the given date
				if ( o->isSolarSystem() ) {
					oldNum = new KSNumbers( ks->data()->ut().djd() );
					o->updateCoords( num, true, geo->lat(), &LST );
				}

				//precess coords to target epoch
				o->updateCoords( num );

				//update pList entry
				pList.replace( i, (SkyPoint*)o );

				KPlotObject *po = new KPlotObject( "", "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );
				for ( double h=-12.0; h<=12.0; h+=0.5 ) {
					po->addPoint( new DPoint( h, findAltitude( o, h ) ) );
				}
				View->replaceObject( i, po );

				//restore original position
				if ( o->isSolarSystem() ) {
					o->updateCoords( oldNum, true, ks->data()->geo()->lat(), ks->LST() );
                                        delete oldNum;
					oldNum = 0;
				}
                                o->EquatorialToHorizontal( ks->LST(), ks->data()->geo()->lat() );

				objFound = true;
				break;
			}
		}

		if ( ! objFound ) {  //assume unfound object is a custom object
			pList.at(i)->updateCoords( num ); //precess to desired epoch

			KPlotObject *po = new KPlotObject( "", "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );
			for ( double h=-12.0; h<=12.0; h+=0.5 ) {
				po->addPoint( new DPoint( h, findAltitude( pList.at(i), h ) ) );
			}
			View->replaceObject( i, po );
		}
	}

	if ( getDate().time().hour() > 12 ) DayOffset = 1;
	else DayOffset = 0;

	setLSTLimits();
	slotHighlight();
	View->repaint();

	delete num;
}

void AltVsTime::slotChooseCity(void) {
	LocationDialog ld(ks);
	if ( ld.exec() == QDialog::Accepted ) {
		GeoLocation *newGeo = ld.selectedCity();
		if ( newGeo ) {
			geo = newGeo;
			avtUI->latBox->showInDegrees( geo->lat() );
			avtUI->longBox->showInDegrees( geo->lng() );
		}
	}
}

int AltVsTime::currentPlotListItem() const {
	return avtUI->PlotList->currentItem();
}

void AltVsTime::setLSTLimits(void) {
	//UT at noon on target date
	KStarsDateTime ut = getDate().addSecs( ((double)DayOffset + 0.5)*86400. );

	dms lst = geo->GSTtoLST( ut.gst() );
	View->setSecondaryLimits( lst.Hours(), lst.Hours() + 24.0, -90.0, 90.0 );
}

void AltVsTime::showCurrentDate (void)
{
	KStarsDateTime dt = KStarsDateTime::currentDateTime();
	if ( dt.time() > QTime( 12, 0, 0 ) ) dt = dt.addDays( 1 );
	avtUI->dateBox->setDate( dt.date() );
}

KStarsDateTime AltVsTime::getDate (void)
{
	//convert midnight local time to UT:
	KStarsDateTime dt( avtUI->dateBox->date(), QTime() );
	dt = geo->LTtoUT( dt );
	return dt;
}

double AltVsTime::getEpoch (QString eName)
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

AVTPlotWidget::AVTPlotWidget( double x1, double x2, double y1, double y2, QWidget *parent, const char* name )
	: KStarsPlotWidget( x1, x2, y1, y2, parent, name )
{
	//Default SunRise/SunSet values
	SunRise = 0.25;
	SunSet = 0.75;
}

void AVTPlotWidget::mousePressEvent( QMouseEvent *e ) {
	mouseMoveEvent( e );
}

void AVTPlotWidget::mouseMoveEvent( QMouseEvent *e ) {
	QRect checkRect( leftPadding(), topPadding(), PixRect.width(), PixRect.height() );
	int Xcursor = e->x();
	int Ycursor = e->y();

	if ( ! checkRect.contains( e->x(), e->y() ) ) {
		if ( e->x() < checkRect.left() )   Xcursor = checkRect.left();
		if ( e->x() > checkRect.right() )  Xcursor = checkRect.right();
		if ( e->y() < checkRect.top() )    Ycursor = checkRect.top();
		if ( e->y() > checkRect.bottom() ) Ycursor = checkRect.bottom();
	}

	Xcursor -= leftPadding();
	Ycursor -= topPadding();

	QPixmap buffer2( *buffer );
	QPainter p;
	p.begin( &buffer2 );
	p.translate( leftPadding(), topPadding() );
	p.setPen( QPen( "grey", 1, SolidLine ) );
	p.drawLine( Xcursor, 0, Xcursor, PixRect.height() );
	p.drawLine( 0, Ycursor, PixRect.width(), Ycursor );
	p.end();
	bitBlt( this, 0, 0, &buffer2 );
}

void AVTPlotWidget::paintEvent( QPaintEvent */*e*/ ) {
	QPainter p;

	p.begin( buffer );
	p.fillRect( 0, 0, width(), height(), bgColor() );

	p.translate( leftPadding(), topPadding() );

	int pW = PixRect.width();
	int pH = PixRect.height();

	//draw daytime sky if the Sun rises for the current date/location
	//(when Sun does not rise, SunSet = -1.0)
	if ( SunSet != -1.0 ) {
		//If Sun does not set, then just fill the daytime sky color
		if ( SunSet == 1.0 ) {
			p.fillRect( 0, 0, pW, int(0.5*pH), QColor( 0, 100, 200 ) );
		} else {
			//Display centered on midnight, so need to modulate dawn/dusk by 0.5
			int dawn = int(pW*(0.5 + SunRise));
			int dusk = int(pW*(SunSet - 0.5));
			p.fillRect( 0, 0, dusk, int(0.5*pH), QColor( 0, 100, 200 ) );
			p.fillRect( dawn, 0, pW - dawn, int(0.5*pH), QColor( 0, 100, 200 ) );

			//draw twilight gradients
			unsigned short int W = 40;
			for ( unsigned short int i=0; i<W; ++i ) {
				p.setPen( QColor( 0, int(100*i/W), 200*i/W ) );
				p.drawLine( dusk - (i-20), 0, dusk - (i-20), pH );
				p.drawLine( dawn + (i-20), 0, dawn + (i-20), pH );
			}
		}
	}

	//draw ground
	p.fillRect( 0, int(0.5*pH), pW, int(0.5*pH), QColor( "#002200" ) );

	drawBox( &p );
	drawObjects( &p );

	//Add vertical line indicating "now"
	QTime t = QTime::currentTime();
	double x = 12.0 + t.hour() + t.minute()/60.0 + t.second()/3600.0;
	while ( x > 24.0 ) x -= 24.0;
	int ix = int(x*pW/24.0); //convert to screen pixel coords
	p.setPen( QPen( "white", 2, DotLine ) );
	p.drawLine( ix, 0, ix, pH );

	p.end();

	bitBlt( this, 0, 0, buffer );
}

#include "altvstime.moc"
