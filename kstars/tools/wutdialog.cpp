/***************************************************************************
                          wutdialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Die Feb 25 2003
    copyright            : (C) 2003 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "wutdialog.h"

#include <QTimer>
#include <QCursor>
#include <QListWidgetItem>

#include <QComboBox>
#include <KLocalizedString>
#include <QPushButton>
#include <KLocalizedString>

#include "ui_wutdialog.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "ksnumbers.h"
#include "simclock.h"
#include "dialogs/detaildialog.h"
#include "dialogs/locationdialog.h"
#include "dialogs/timedialog.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "skycomponents/skymapcomposite.h"

WUTDialogUI::WUTDialogUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

WUTDialog::WUTDialog( QWidget *parent, bool _session, GeoLocation *_geo, KStarsDateTime _lt ) :
        QDialog( parent ),
        session(_session),
        T0(_lt),
        geo(_geo),
        EveningFlag(0),
        timer(NULL)
{
    WUT = new WUTDialogUI( this );

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(WUT);
    setLayout(mainLayout);

    setWindowTitle( xi18n("What's up Tonight") );

    //FIXME Needs to be ported to KF5
    //setMainWidget( WUT );
    //setButtons( QDialog::Close );

    setModal( false );

    //If the Time is earlier than 6:00 am, assume the user wants the night of the previous date
    if ( T0.time().hour() < 6 )
        T0 = T0.addDays( -1 );

    //Now, set time T0 to midnight (of the following day)
    T0.setTime( QTime( 0, 0, 0 ) );
    T0 = T0.addDays( 1 );
    UT0 = geo->LTtoUT( T0 );

    //Set the Tomorrow date/time to Noon the following day
    Tomorrow = T0.addSecs( 12*3600 );
    TomorrowUT = geo->LTtoUT( Tomorrow );

    //Set the Evening date/time to 6:00 pm
    Evening = T0.addSecs( -6*3600 );
    EveningUT = geo->LTtoUT( Evening );

    QString sGeo = geo->translatedName();
    if ( ! geo->translatedProvince().isEmpty() ) sGeo += ", " + geo->translatedProvince();
    sGeo += ", " + geo->translatedCountry();
    WUT->LocationLabel->setText( xi18n( "at %1", sGeo ) );
    //FIXME Need porting to KF5
    //WUT->DateLabel->setText( xi18n( "The night of %1", QLocale().toString( Evening.date(), KLocale::LongDate ) ) );
    m_Mag = 10.0;
    WUT->MagnitudeEdit->setValue( m_Mag );
    //WUT->MagnitudeEdit->setSliderEnabled( true );
    WUT->MagnitudeEdit->setSingleStep(0.100);
    initCategories();

    makeConnections();

    QTimer::singleShot(0, this, SLOT(init()));
}

WUTDialog::~WUTDialog(){
}

void WUTDialog::makeConnections() {
    connect( WUT->DateButton, SIGNAL( clicked() ), SLOT( slotChangeDate() ) );
    connect( WUT->LocationButton, SIGNAL( clicked() ), SLOT( slotChangeLocation() ) );
    connect( WUT->CenterButton, SIGNAL( clicked() ), SLOT( slotCenter() ) );
    connect( WUT->DetailButton, SIGNAL( clicked() ), SLOT( slotDetails() ) );
    connect( WUT->ObslistButton, SIGNAL( clicked() ), SLOT( slotObslist() ) );
    connect( WUT->CategoryListWidget, SIGNAL( currentTextChanged(const QString &) ),
             SLOT( slotLoadList(const QString &) ) );
    connect( WUT->ObjectListWidget, SIGNAL( currentTextChanged(const QString &) ),
             SLOT( slotDisplayObject(const QString &) ) );
    connect( WUT->EveningMorningBox, SIGNAL( activated(int) ), SLOT( slotEveningMorning(int) ) );
    connect( WUT->MagnitudeEdit, SIGNAL( valueChanged( double ) ), SLOT( slotChangeMagnitude() ) );
}

void WUTDialog::initCategories() {
    m_Categories << xi18n( "Planets" ) << xi18n( "Stars" )
    << xi18n( "Nebulae" ) << xi18n( "Galaxies" )
    << xi18n( "Star Clusters" ) << xi18n( "Constellations" ) 
    << xi18n( "Asteroids" ) << xi18n( "Comets" );

    foreach ( const QString &c, m_Categories )
    WUT->CategoryListWidget->addItem( c );

    WUT->CategoryListWidget->setCurrentRow( 0 );
}

void WUTDialog::init() {
    QString sRise, sSet, sDuration;
    float Dur;
    int hDur,mDur;
    KStarsData* data = KStarsData::Instance();
    // reset all lists
    foreach ( const QString &c, m_Categories ) {
        if ( m_VisibleList.contains( c ) )
            visibleObjects( c ).clear();
        else
            m_VisibleList[ c ] = QList<SkyObject*>();

        m_CategoryInitialized[ c ] = false;
    }

    // sun almanac information
    KSSun *oSun = dynamic_cast<KSSun*>( data->objectNamed("Sun") );
    sunRiseTomorrow = oSun->riseSetTime( TomorrowUT, geo, true );
    sunSetToday = oSun->riseSetTime( EveningUT, geo, false );
    sunRiseToday = oSun->riseSetTime( EveningUT, geo, true );

    //check to see if Sun is circumpolar
    KSNumbers *num = new KSNumbers( UT0.djd() );
    KSNumbers *oldNum = new KSNumbers( data->ut().djd() );
    dms LST = geo->GSTtoLST( T0.gst() );

    oSun->updateCoords( num, true, geo->lat(), &LST );
    if ( oSun->checkCircumpolar( geo->lat() ) ) {
        if ( oSun->alt().Degrees() > 0.0 ) {
            sRise = xi18n( "circumpolar" );
            sSet = xi18n( "circumpolar" );
            sDuration = "00:00";
            Dur = hDur = mDur = 0;
        } else {
            sRise = xi18n( "does not rise" );
            sSet = xi18n( "does not rise" );
            sDuration = "24:00";
            Dur = hDur = 24;
            mDur = 0;
        }
    } else {
        //Round times to the nearest minute by adding 30 seconds to the time
        sRise = QLocale().toString( sunRiseTomorrow );
        sSet = QLocale().toString( sunSetToday );

        Dur = 24.0 + (float)sunRiseTomorrow.hour()
                    + (float)sunRiseTomorrow.minute()/60.0
                    + (float)sunRiseTomorrow.second()/3600.0
                    - (float)sunSetToday.hour()
                    - (float)sunSetToday.minute()/60.0
                    - (float)sunSetToday.second()/3600.0;
        hDur = int(Dur);
        mDur = int(60.0*(Dur - (float)hDur));
        QTime tDur( hDur, mDur );
        //TODO Need Check
        //sDuration = QLocale().toString( tDur, false, true );
        sDuration = QLocale().toString( tDur);
    }

    //FIXME Needs porting to KF5
    //WUT->SunSetLabel->setText( xi18nc( "Sunset at time %1 on date %2", "Sunset: %1 on %2" , sSet, QLocale().toString( Evening.date(), KLocale::LongDate) ) );
    //WUT->SunRiseLabel->setText( xi18nc( "Sunrise at time %1 on date %2", "Sunrise: %1 on %2" , sRise, QLocale().toString( Tomorrow.date(), KLocale::LongDate) ) );
    if( Dur == 0 )
        WUT->NightDurationLabel->setText( xi18n("Night duration: %1", sDuration ) );
    else if( Dur > 1 )
        WUT->NightDurationLabel->setText( xi18n("Night duration: %1 hours", sDuration ) );
    else if( Dur == 1 )
        WUT->NightDurationLabel->setText( xi18n("Night duration: %1 hour", sDuration ) );
    else if( mDur > 1 )
        WUT->NightDurationLabel->setText( xi18n("Night duration: %1 minutes", sDuration ) );
    else if( mDur == 1 )
        WUT->NightDurationLabel->setText( xi18n("Night duration: %1 minute", sDuration ) );

    // moon almanac information
    KSMoon *oMoon = dynamic_cast<KSMoon*>( data->objectNamed("Moon") );
    moonRise = oMoon->riseSetTime( UT0, geo, true );
    moonSet = oMoon->riseSetTime( UT0, geo, false );

    //check to see if Moon is circumpolar
    oMoon->updateCoords( num, true, geo->lat(), &LST );
    if ( oMoon->checkCircumpolar( geo->lat() ) ) {
        if ( oMoon->alt().Degrees() > 0.0 ) {
            sRise = xi18n( "circumpolar" );
            sSet = xi18n( "circumpolar" );
        } else {
            sRise = xi18n( "does not rise" );
            sSet = xi18n( "does not rise" );
        }
    } else {
        //Round times to the nearest minute by adding 30 seconds to the time
        sRise = QLocale().toString( moonRise.addSecs(30) );
        sSet = QLocale().toString( moonSet.addSecs(30) );
    }

    //FIXME Needs porting to KF5
    //WUT->MoonRiseLabel->setText( xi18n( "Moon rises at: %1 on %2", sRise, QLocale().toString( Evening.date(), KLocale::LongDate) ) );

    // If the moon rises and sets on the same day, this will be valid [ Unless
    // the moon sets on the next day after staying on for over 24 hours ]

    /* FIXME Needs porting to KF5
    if( moonSet > moonRise )
        WUT->MoonSetLabel->setText( xi18n( "Moon sets at: %1 on %2", sSet, QLocale().toString( Evening.date(), KLocale::LongDate) ) );
    else
        WUT->MoonSetLabel->setText( xi18n( "Moon sets at: %1 on %2", sSet, QLocale().toString( Tomorrow.date(), KLocale::LongDate) ) );
        */
    oMoon->findPhase();
    WUT->MoonIllumLabel->setText( oMoon->phaseName() + QString( " (%1%)" ).arg(
                                      int(100.0*oMoon->illum() ) ) );

    //Restore Sun's and Moon's coordinates, and recompute Moon's original Phase
    oMoon->updateCoords( oldNum, true, geo->lat(), data->lst() );
    oSun->updateCoords( oldNum, true, geo->lat(), data->lst() );
    oMoon->findPhase();

    if ( WUT->CategoryListWidget->currentItem() )
        slotLoadList( WUT->CategoryListWidget->currentItem()->text() );

    delete num;
    delete oldNum;
}

QList<SkyObject*>& WUTDialog::visibleObjects( const QString &category ) {
    return m_VisibleList[ category ];
}

bool WUTDialog::isCategoryInitialized( const QString &category ) {
    return m_CategoryInitialized[ category ];
}

void WUTDialog::slotLoadList( const QString &c ) {
    KStarsData* data = KStarsData::Instance();
    if ( ! m_VisibleList.contains( c ) )
        return;

    WUT->ObjectListWidget->clear();
    setCursor(QCursor(Qt::WaitCursor));

    if ( ! isCategoryInitialized(c) ) {

        if ( c == m_Categories[0] ) { //Planets
            foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANET ) ) {
                SkyObject *o = data->skyComposite()->findByName( name );

                if ( checkVisibility( o ) && o->mag() <= m_Mag )
                    visibleObjects(c).append(o);
            }

            m_CategoryInitialized[ c ] = true;
        }

        else if ( c == m_Categories[1] ) { //Stars
            foreach ( SkyObject *o, data->skyComposite()->stars() )
            if ( o->name() != xi18n("star") && checkVisibility(o) && o->mag() <= m_Mag )
                visibleObjects(c).append(o);

            m_CategoryInitialized[ c ] = true;
        }

        else if ( c == m_Categories[5] ) { //Constellations
            foreach ( SkyObject *o, data->skyComposite()->constellationNames() )
            if ( checkVisibility(o) )
                visibleObjects(c).append(o);

            m_CategoryInitialized[ c ] = true;
        }

        else if ( c == m_Categories[6] ) { //Asteroids
            foreach ( SkyObject *o, data->skyComposite()->asteroids() )
            if ( checkVisibility(o) && o->name() != xi18n("Pluto") && o->mag() <= m_Mag )
                visibleObjects(c).append(o);

            m_CategoryInitialized[ c ] = true;
        }

        else if ( c == m_Categories[7] ) { //Comets
            foreach ( SkyObject *o, data->skyComposite()->comets() )
            if ( checkVisibility(o) )
                visibleObjects(c).append(o);

            m_CategoryInitialized[ c ] = true;
        }

        else { //all deep-sky objects, need to split clusters, nebulae and galaxies
            foreach ( DeepSkyObject *dso, data->skyComposite()->deepSkyObjects() ) {
                SkyObject *o = (SkyObject*)dso;
                if ( checkVisibility(o) && o->mag() <= m_Mag ) {
                    switch( o->type() ) {
                    case SkyObject::OPEN_CLUSTER: //fall through
                    case SkyObject::GLOBULAR_CLUSTER:
                        visibleObjects(m_Categories[4]).append(o); //star clusters
                        break;
                    case SkyObject::GASEOUS_NEBULA: //fall through
                    case SkyObject::PLANETARY_NEBULA: //fall through
                    case SkyObject::SUPERNOVA_REMNANT:
                        visibleObjects(m_Categories[2]).append(o); //nebulae
                        break;
                    case SkyObject::GALAXY:
                        visibleObjects(m_Categories[3]).append(o); //galaxies
                        break;
                    }
                }
            }

            m_CategoryInitialized[ m_Categories[2] ] = true;
            m_CategoryInitialized[ m_Categories[3] ] = true;
            m_CategoryInitialized[ m_Categories[4] ] = true;
        }
    }

    //Now the category has been initialized, we can populate the list widget
    foreach ( SkyObject *o, visibleObjects(c) )
    WUT->ObjectListWidget->addItem( o->name() );

    setCursor(QCursor(Qt::ArrowCursor));

    // highlight first item
    if ( WUT->ObjectListWidget->count() ) {
        WUT->ObjectListWidget->setCurrentRow( 0 );
        WUT->ObjectListWidget->setFocus();
    }
}

bool WUTDialog::checkVisibility(SkyObject *o) {
    bool visible( false );
    double minAlt = 6.0; //An object is considered 'visible' if it is above horizon during civil twilight.

    // reject objects that never rise
    if (o->checkCircumpolar(geo->lat()) == true && o->alt().Degrees() <= 0) return false;

    //Initial values for T1, T2 assume all night option of EveningMorningBox
    KStarsDateTime T1 = Evening;
    T1.setTime( sunSetToday );
    KStarsDateTime T2 = Tomorrow;
    T2.setTime( sunRiseTomorrow );

    //Check Evening/Morning only state:
    if ( EveningFlag==0 ) { //Evening only
        T2 = T0; //midnight
    } else if ( EveningFlag==1 ) { //Morning only
        T1 = T0; //midnight
    }

    for ( KStarsDateTime test = T1; test < T2; test = test.addSecs(3600) ) {
        //Need LST of the test time, expressed as a dms object.
        KStarsDateTime ut = geo->LTtoUT( test );
        dms LST = geo->GSTtoLST( ut.gst() );
        SkyPoint sp = o->recomputeCoords( ut, geo );

        //check altitude of object at this time.
        sp.EquatorialToHorizontal( &LST, geo->lat() );

        if ( sp.alt().Degrees() > minAlt ) {
            visible = true;
            break;
        }
    }

    return visible;
}

void WUTDialog::slotDisplayObject( const QString &name ) {
    QTime tRise, tSet, tTransit;
    QString sRise, sTransit, sSet;

    sRise = "--:--";
    sTransit = "--:--";
    sSet = "--:--";
    WUT->DetailButton->setEnabled( false );

    SkyObject *o = 0;
    if ( name.isEmpty() )
    {  //no object selected
        WUT->ObjectBox->setTitle( xi18n( "No Object Selected" ) );
        o = 0;
    } else {
        o = KStarsData::Instance()->objectNamed( name );

        if ( !o ) { //should never get here
            WUT->ObjectBox->setTitle( xi18n( "Object Not Found" ) );
        }
    }
    if (o) {
        WUT->ObjectBox->setTitle( o->name() );

        if ( o->checkCircumpolar( geo->lat() ) ) {
            if ( o->alt().Degrees() > 0.0 ) {
                sRise = xi18n( "circumpolar" );
                sSet = xi18n( "circumpolar" );
            } else {
                sRise = xi18n( "does not rise" );
                sSet = xi18n( "does not rise" );
            }
        } else {
            tRise = o->riseSetTime( T0, geo, true );
            tSet = o->riseSetTime( T0, geo, false );
            //          if ( tSet < tRise )
            //              tSet = o->riseSetTime( JDTomorrow, geo, false );

            sRise.clear();
            sRise.sprintf( "%02d:%02d", tRise.hour(), tRise.minute() );
            sSet.clear();
            sSet.sprintf( "%02d:%02d", tSet.hour(), tSet.minute() );
        }

        tTransit = o->transitTime( T0, geo );
        //      if ( tTransit < tRise )
        //          tTransit = o->transitTime( JDTomorrow, geo );

        sTransit.clear();
        sTransit.sprintf( "%02d:%02d", tTransit.hour(), tTransit.minute() );

        WUT->DetailButton->setEnabled( true );
    }

    WUT->ObjectRiseLabel->setText( xi18n( "Rises at: %1", sRise ) );
    WUT->ObjectTransitLabel->setText( xi18n( "Transits at: %1", sTransit ) );
    WUT->ObjectSetLabel->setText( xi18n( "Sets at: %1", sSet ) );
}

void WUTDialog::slotCenter() {
    KStars* kstars = KStars::Instance();
    SkyObject *o = 0;
    // get selected item
    if (WUT->ObjectListWidget->currentItem() != 0) {
        o = kstars->data()->objectNamed( WUT->ObjectListWidget->currentItem()->text() );
    }
    if (o != 0) {
        kstars->map()->setFocusPoint( o );
        kstars->map()->setFocusObject( o );
        kstars->map()->setDestination( *kstars->map()->focusPoint() );
    }
}

void WUTDialog::slotDetails() {
    KStars* kstars = KStars::Instance();
    SkyObject *o = 0;
    // get selected item
    if (WUT->ObjectListWidget->currentItem() != 0) {
        o = kstars->data()->objectNamed( WUT->ObjectListWidget->currentItem()->text() );
    }
    if (o != 0) {
        QPointer<DetailDialog> detail = new DetailDialog(o, kstars->data()->lt(), geo, kstars);
        detail->exec();
	delete detail;
    }
}
void WUTDialog::slotObslist() {
    KStars* kstars = KStars::Instance();
    SkyObject *o = 0;
    // get selected item
    if (WUT->ObjectListWidget->currentItem() != 0) {
        o = kstars->data()->objectNamed( WUT->ObjectListWidget->currentItem()->text() );
    }
    if(o != 0) {
        kstars->observingList()->slotAddObject( o, session ) ;
    }
}

void WUTDialog::slotChangeDate() {

    // Set the time T0 to the evening of today. This will make life easier for the user, who most probably 
    // wants to see what's up on the night of some date, rather than the night of the previous day
    T0.setTime( QTime( 18, 0, 0 ) ); // 6 PM

    QPointer<TimeDialog> td = new TimeDialog( T0, KStarsData::Instance()->geo(), this );
    if ( td->exec() == QDialog::Accepted ) {
        T0 = td->selectedDateTime();

        // If the time is set to 12 midnight, set it to 00:00:01, so that we don't have date interpretation problems
        if ( T0.time() == QTime( 0, 0, 0 ) )
            T0.setTime( QTime( 0, 0, 1 ) );

        //If the Time is earlier than 6:00 am, assume the user wants the night of the previous date
        if ( T0.time().hour() < 6 )
            T0 = T0.addDays( -1 );

        //Now, set time T0 to midnight (of the following day)
        T0.setTime( QTime( 0, 0, 0 ) );
        T0 = T0.addDays( 1 );
        UT0 = geo->LTtoUT( T0 );

        //Set the Tomorrow date/time to Noon the following day
        Tomorrow = T0.addSecs( 12*3600 );
        TomorrowUT = geo->LTtoUT( Tomorrow );

        //Set the Evening date/time to 6:00 pm
        Evening = T0.addSecs( -6*3600 );
        EveningUT = geo->LTtoUT( Evening );

        //FIXME Needs porting to KF5
        //WUT->DateLabel->setText( xi18n( "The night of %1", QLocale().toString( Evening.date(), KLocale::LongDate ) ) );

        init();
        slotLoadList( WUT->CategoryListWidget->currentItem()->text() );
    }
    delete td;
}

void WUTDialog::slotChangeLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        GeoLocation *newGeo = ld->selectedCity();
        if ( newGeo ) {
            geo = newGeo;
            UT0 = geo->LTtoUT( T0 );
            TomorrowUT = geo->LTtoUT( Tomorrow );
            EveningUT = geo->LTtoUT( Evening );

            WUT->LocationLabel->setText( xi18n( "at %1", geo->fullName() ) );

            init();
            slotLoadList( WUT->CategoryListWidget->currentItem()->text() );
        }
    }
    delete ld;
}

void WUTDialog::slotEveningMorning( int index ) {
    if ( EveningFlag != index ) {
        EveningFlag = index;
        init();
        slotLoadList( WUT->CategoryListWidget->currentItem()->text() );
    }
}

void WUTDialog::updateMag() {
    m_Mag = WUT->MagnitudeEdit->value();
    init();
    slotLoadList( WUT->CategoryListWidget->currentItem()->text() );
}

void WUTDialog::slotChangeMagnitude() {
    if( timer ) {
        timer->stop();
    } else {
        timer = new QTimer( this );
        timer->setSingleShot( true );
        connect( timer, SIGNAL( timeout() ), this, SLOT( updateMag() ) );
    }
    timer->start( 500 );
}

