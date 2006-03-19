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

#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QPixmap>
#include <QFrame>
#include <QPaintEvent>
#include <QPointF>

#include <klocale.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>
#include <kdialogbase.h>

#include "altvstime.h"
#include "altvstimeui.h"
#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skypoint.h"
#include "skyobject.h"
#include "ksnumbers.h"
#include "simclock.h"
#include "finddialog.h"
#include "locationdialog.h"
#include "widgets/dmsbox.h"
#include "widgets/kstarsplotwidget.h"

#include "kstarsdatetime.h"
#include "libkdeedu/extdate/extdatetimeedit.h"
#include "libkdeedu/kdeeduplot/kplotobject.h"
#include "libkdeedu/kdeeduplot/kplotaxis.h"
#include "libkdeedu/kdeeduplot/kplotwidget.h"

AltVsTimeUI::AltVsTimeUI( QWidget *p ) : QFrame( p ) {
	setupUi( this );
}

AltVsTime::AltVsTime( QWidget* parent)  :
	KDialog( parent, i18n( "Altitude vs. Time" ), KDialog::Close )
{
	ks = (KStars*) parent;

	QFrame *page = new QFrame( this );
	setMainWidget(page);

	topLayout = new QVBoxLayout( page, 0, spacingHint() );

	View = new AVTPlotWidget( -12.0, 12.0, -90.0, 90.0, page );
	View->setMinimumSize( 400, 400 );
	View->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
	View->setXAxisType( KStarsPlotWidget::TIME );
	View->setYAxisType( KStarsPlotWidget::ANGLE );
	View->setShowGrid( false );
	View->axis(KPlotWidget::BottomAxis)->setLabel( i18n( "Local Time" ) );
	View->setXAxisLabel2( i18n( "Local Sidereal Time" ) );
	View->axis(KPlotWidget::LeftAxis)->setLabel( i18n( "the angle of an object above (or below) the horizon", "Altitude" ) );

	avtUI = new AltVsTimeUI( page );
	avtUI->raBox->setDegType( false );
	avtUI->decBox->setDegType( true );

	//FIXME:
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

	//ther edit boxes should not pass on the return key!
	avtUI->nameBox->setTrapReturnKey( true );
	avtUI->raBox->setTrapReturnKey( true );
	avtUI->decBox->setTrapReturnKey( true );

	setMouseTracking( true );
}

AltVsTime::~AltVsTime()
{
    //WARNING: need to delete deleteList items!
}

void AltVsTime::slotAddSource(void) {
	SkyObject *obj = ks->data()->objectNamed( avtUI->nameBox->text() );

	if ( obj ) {
		//An object with the current name exists.  If the object is not already 
		//in the avt list, add it.
		bool found(false);
		foreach ( SkyObject *o, pList ) {
			if ( o->name() == obj->name() ) { found = true; break; }
		}

		if ( found )
			kDebug() << i18n("An object named %1 is already displayed; I will not duplicate it.").arg(obj->name()) << endl;

		else 
			processObject( obj );

	} else {
		//Object with the current name doesn't exist.  It's possible that the 
		//user is trying to add a custom object.  Assume this is the case if 
		//the RA and Dec fields are filled in.

		if ( ! avtUI->nameBox->text().isEmpty() && 
				! avtUI->raBox->text().isEmpty() && 
				! avtUI->decBox->text().isEmpty() ) {
			bool ok( true );
			dms newRA( 0.0 ), newDec( 0.0 );
			newRA = avtUI->raBox->createDms( false, &ok );
			if ( ok ) newDec = avtUI->decBox->createDms( true, &ok );
			if ( !ok ) return;
	
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
			foreach ( SkyObject *p, pList ) {
				//within an arcsecond?
				if ( fabs( newRA.Degrees() - p->ra()->Degrees() ) < 0.0003 && fabs( newDec.Degrees() - p->dec()->Degrees() ) < 0.0003 ) {
					found = true;
					break;
				}
			}
			if ( found ) {
				kDebug() << "This point is already displayed; I will not duplicate it." << endl;
				ok = false;
			}
	
			if ( ok ) {
				SkyObject *obj = new SkyObject( 8, newRA, newDec, 1.0, avtUI->nameBox->text() );
				deleteList.append( obj ); //this object will be deleted when window is destroyed
				processObject( obj );
			}
		}

		//If the Ra and Dec boxes are filled, but the name field is empty,
		//move input focus to nameBox.  If either coordinate box is empty, 
		//move focus there
		if ( avtUI->nameBox->text().isEmpty() ) 
			avtUI->nameBox->QWidget::setFocus();
		if ( avtUI->raBox->text().isEmpty() ) 
			avtUI->raBox->QWidget::setFocus();
		else if ( avtUI->decBox->text().isEmpty() ) 
			avtUI->decBox->QWidget::setFocus();
	}

	View->repaint(false);
}

//Use find dialog to choose an object
void AltVsTime::slotBrowseObject(void) {
	FindDialog fd(ks);
	if ( fd.exec() == QDialog::Accepted ) {
		SkyObject *o = fd.currentItem();
		processObject( o );
	}

	View->repaint();
}

void AltVsTime::processObject( SkyObject *o, bool forceAdd ) {
	if ( !o ) return;

	KSNumbers *num = new KSNumbers( getDate().djd() );
	KSNumbers *oldNum = 0;

	//If the object is in the solar system, recompute its position for the given epochLabel
	if ( o->isSolarSystem() ) {
		oldNum = new KSNumbers( ks->data()->ut().djd() );
		o->updateCoords( num, true, geo->lat(), ks->LST() );
	}

	//precess coords to target epoch
	o->updateCoords( num );

	//If this point is not in list already, add it to list
	bool found(false);
	foreach ( SkyObject *p, pList ) {
		if ( o->ra()->Degrees() == p->ra()->Degrees() && o->dec()->Degrees() == p->dec()->Degrees() ) {
			found = true;
			break;
		}
	}
	if ( found && !forceAdd ) kDebug() << "This point is already displayed; I will not duplicate it." << endl;
	else {
		pList.append( o );

		//make sure existing curves are thin and red
		for ( int i=0; i < View->objectCount(); ++i ) {
			KPlotObject *obj = View->object( i );
			if ( obj->size() == 2 ) {
				obj->setColor( "red" );
				obj->setSize( 1 );
			}
		}

		//add new curve with width=2, and color=white
		KPlotObject *po = new KPlotObject( QString(), "white", KPlotObject::CURVE, 2, KPlotObject::SOLID );
		for ( double h=-12.0; h<=12.0; h+=0.5 ) {
			po->addPoint( new QPointF( h, findAltitude( o, h ) ) );
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
	kDebug() << "Currently, there are " << View->objectCount() << " objects displayed." << endl;

	//restore original position
	if ( o->isSolarSystem() ) {
		o->updateCoords( oldNum, true, ks->geo()->lat(), ks->LST() );
		delete oldNum;
	}
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

	for ( int i=0; i < pList.size(); ++i ) {
		if ( i == iPlotList ) {
			SkyObject *p = pList.at(i);
			avtUI->raBox->showInHours( p->ra() );
			avtUI->decBox->showInDegrees( p->dec() );
			avtUI->nameBox->setText( avtUI->PlotList->currentText() );
		}
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
	//Need to delete the pointers in deleteList
	while ( ! deleteList.isEmpty() ) 
		delete deleteList.takeFirst();
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
	
	SkyObject *oSun = ks->data()->objectNamed( "Sun" );
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

		SkyObject *o = ks->data()->objectNamed( oName );
		if ( o ) {
			//If the object is in the solar system, recompute its position for the given date
			if ( o->isSolarSystem() ) {
				oldNum = new KSNumbers( ks->data()->ut().djd() );
				o->updateCoords( num, true, geo->lat(), &LST );
			}

			//precess coords to target epoch
			o->updateCoords( num );

			//update pList entry
			pList.replace( i, o );

			KPlotObject *po = new KPlotObject( QString(), "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );
			for ( double h=-12.0; h<=12.0; h+=0.5 ) {
				po->addPoint( new QPointF( h, findAltitude( o, h ) ) );
			}
			View->replaceObject( i, po );

			//restore original position
			if ( o->isSolarSystem() ) {
				o->updateCoords( oldNum, true, ks->data()->geo()->lat(), ks->LST() );
				delete oldNum;
				oldNum = 0;
			}
		} else {  //assume unfound object is a custom object
			pList.at(i)->updateCoords( num ); //precess to desired epoch

			KPlotObject *po = new KPlotObject( QString(), "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );
			for ( double h=-12.0; h<=12.0; h+=0.5 ) {
				po->addPoint( new QPointF( h, findAltitude( pList.at(i), h ) ) );
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

double AltVsTime::getEpoch(const QString &eName)
{
	//If Epoch field not a double, assume J2000
	bool ok(false);
	double epoch = eName.toDouble(&ok);

	if ( !ok ) {
		kDebug() << "Invalid Epoch.  Assuming 2000.0." << endl;
		return 2000.0;
	}

	return epoch;
}

AVTPlotWidget::AVTPlotWidget( double x1, double x2, double y1, double y2, QWidget *parent )
	: KStarsPlotWidget( x1, x2, y1, y2, parent )
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
	p.setPen( QPen( QBrush("grey"), 1.0, Qt::SolidLine ) );
	p.drawLine( Xcursor, 0, Xcursor, PixRect.height() );
	p.drawLine( 0, Ycursor, PixRect.width(), Ycursor );
	p.end();
	bitBlt( this, 0, 0, &buffer2 );
}

void AVTPlotWidget::paintEvent( QPaintEvent */*e*/ ) {
	QPainter p;

	p.begin( buffer );
	p.fillRect( 0, 0, width(), height(), backgroundColor() );

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
	p.setPen( QPen( QBrush("white"), 2.0, Qt::DotLine ) );
	p.drawLine( ix, 0, ix, pH );

	p.end();

	bitBlt( this, 0, 0, buffer );
}

#include "altvstime.moc"
