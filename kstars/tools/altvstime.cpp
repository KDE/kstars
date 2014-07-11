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

#include "altvstime.h"

#include <QVBoxLayout>
#include <QFrame>

#include <KLocalizedString>
#include <QDialog>
#include <kplotobject.h>
#include <kplotwidget.h>
#include <kplotaxis.h>
#include <QPainter>
#include <kdeprintdialog.h>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>

#include "dms.h"
#include "ksalmanac.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "simclock.h"
#include "kssun.h"
#include "dialogs/finddialog.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skypoint.h"
#include "skyobjects/skyobject.h"

#include "avtplotwidget.h"
#include "ui_altvstime.h"

AltVsTimeUI::AltVsTimeUI( QWidget *p ) :
    QFrame( p )
{
    setupUi( this );
}

AltVsTime::AltVsTime( QWidget* parent)  :
    QDialog( parent )
{
    QFrame *page = new QFrame( this );

    //setMainWidget(page);
    //setWindowTitle( xi18n( "Altitude vs. Time" ) );
    setWindowTitle(xi18n( "Altitude vs. Time" ) );


    // FIXME How do we migrate this?
    //setButtons( QDialog::Close | QDialog::User1 );
    //setButtonGuiItem( QDialog::User1, KGuiItem( xi18n("&Print..."), "document-print", xi18n("Print the Altitude vs. time plot") ) );
    setModal( false );


    QVBoxLayout* topLayout = new QVBoxLayout;
    topLayout->setMargin( 0 );
    topLayout->addWidget(page);
    //topLayout->setSpacing( spacingHint() );

    //avtUI = new AltVsTimeUI( page );
    avtUI = new AltVsTimeUI( page );

    avtUI->View->setLimits( -12.0, 12.0, -90.0, 90.0 );
    avtUI->View->setShowGrid( false );
    avtUI->View->axis(KPlotWidget::BottomAxis)->setTickLabelFormat( 't' );
    avtUI->View->axis(KPlotWidget::BottomAxis)->setLabel( xi18n( "Local Time" ) );
    avtUI->View->axis(KPlotWidget::TopAxis)->setTickLabelFormat( 't' );
    avtUI->View->axis(KPlotWidget::TopAxis)->setTickLabelsShown( true );
    avtUI->View->axis(KPlotWidget::TopAxis)->setLabel( xi18n( "Local Sidereal Time" ) );
    avtUI->View->axis(KPlotWidget::LeftAxis)->setLabel( xi18nc( "the angle of an object above (or below) the horizon", "Altitude" ) );

    avtUI->raBox->setDegType( false );
    avtUI->decBox->setDegType( true );

    //FIXME:
    //Doesn't make sense to manually adjust long/lat unless we can modify TZ also
    avtUI->longBox->setReadOnly( true );
    avtUI->latBox->setReadOnly( true );

    //topLayout->addWidget( avtUI );

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    topLayout->addWidget(buttons);

    QPushButton *okButton = new QPushButton(QIcon::fromTheme("window-close"), xi18n("Close"));
    buttons->addButton(okButton, QDialogButtonBox::AcceptRole);
    connect(okButton, SIGNAL(clicked()), this, SLOT(slotOk()));

    QPushButton *printButton = new QPushButton(QIcon::fromTheme("document-print"), xi18n("&Print..."));
    printButton->setToolTip(xi18n("Print the Altitude vs. time plot."));
    buttons->addButton(printButton, QDialogButtonBox::ApplyRole);
    connect(printButton, SIGNAL(clicked()), this, SLOT(slotPrint()));

    geo = KStarsData::Instance()->geo();

    DayOffset = 0;
    showCurrentDate();
    if ( getDate().time().hour() > 12 )
        DayOffset = 1;

    avtUI->longBox->show( geo->lng() );
    avtUI->latBox->show( geo->lat() );

    computeSunRiseSetTimes();
    setLSTLimits();
    setDawnDusk();

    connect( avtUI->browseButton, SIGNAL( clicked() ), this, SLOT( slotBrowseObject() ) );
    connect( avtUI->cityButton,   SIGNAL( clicked() ), this, SLOT( slotChooseCity() ) );
    connect( avtUI->updateButton, SIGNAL( clicked() ), this, SLOT( slotUpdateDateLoc() ) );
    connect( avtUI->clearButton , SIGNAL( clicked() ), this, SLOT( slotClear() ) );
    connect( avtUI->addButton,    SIGNAL( clicked() ), this, SLOT( slotAddSource() ) );
    connect( avtUI->nameBox, SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
    connect( avtUI->raBox,   SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
    connect( avtUI->decBox,  SIGNAL( returnPressed() ), this, SLOT( slotAddSource() ) );
    connect( avtUI->clearFieldsButton, SIGNAL( clicked() ), this, SLOT( slotClearBoxes() ) );
    connect( avtUI->longBox, SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
    connect( avtUI->latBox,  SIGNAL( returnPressed() ), this, SLOT( slotAdvanceFocus() ) );
    connect( avtUI->PlotList, SIGNAL( currentRowChanged(int) ), this, SLOT( slotHighlight(int) ) );
    // FIXME
    //connect( button( QDialog::User1 ), SIGNAL( clicked() ), this, SLOT( slotPrint() ) );

    //the edit boxes should not pass on the return key!
    // FIXME TODO
    //avtUI->nameBox->setTrapReturnKey( true );
    //avtUI->raBox->setTrapReturnKey( true );
    //avtUI->decBox->setTrapReturnKey( true );

    setMouseTracking( true );
}

AltVsTime::~AltVsTime()
{
    //WARNING: need to delete deleteList items!
}

void AltVsTime::slotAddSource() {
    SkyObject *obj = KStarsData::Instance()->objectNamed( avtUI->nameBox->text() );

    if ( obj ) {
        //An object with the current name exists.  If the object is not already
        //in the avt list, add it.
        bool found = false;
        foreach ( SkyObject *o, pList ) {
            //if ( o->name() == obj->name() ) {
            if ( getObjectName(o, false) == getObjectName(obj, false) ) {
                found = true;
                break;
            }
        }
        if( !found )
            processObject( obj );
    } else {
        //Object with the current name doesn't exist.  It's possible that the
        //user is trying to add a custom object.  Assume this is the case if
        //the RA and Dec fields are filled in.

        if( ! avtUI->nameBox->text().isEmpty() &&
            ! avtUI->raBox->text().isEmpty()   &&
            ! avtUI->decBox->text().isEmpty() )
        {
            bool okRA, okDec;
            dms newRA  = avtUI->raBox->createDms( false, &okRA );
            dms newDec = avtUI->decBox->createDms( true, &okDec );
            if( !okRA || !okDec )
                return;

            //If the epochName is blank (or any non-double), we assume J2000
            //Otherwise, precess to J2000.
            KStarsDateTime dt;
            dt.setFromEpoch( getEpoch( avtUI->epochName->text() ) );
            long double jd = dt.djd();
            if ( jd != J2000 ) {
                SkyPoint ptest( newRA, newDec );
                ptest.precessFromAnyEpoch( jd, J2000 );
                newRA.setH( ptest.ra().Hours() );
                newDec.setD( ptest.dec().Degrees() );
            }

            //make sure the coords do not already exist from another object
            bool found = false;
            foreach ( SkyObject *p, pList ) {
                //within an arcsecond?
                if ( fabs( newRA.Degrees() - p->ra().Degrees() ) < 0.0003 && fabs( newDec.Degrees() - p->dec().Degrees() ) < 0.0003 ) {
                    found = true;
                    break;
                }
            }
            if( !found ) {
                SkyObject *obj = new SkyObject( 8, newRA, newDec, 1.0, avtUI->nameBox->text() );
                deleteList.append( obj ); //this object will be deleted when window is destroyed
                processObject( obj );
            }
        }

        //If the Ra and Dec boxes are filled, but the name field is empty,
        //move input focus to nameBox.  If either coordinate box is empty,
        //move focus there
        if( avtUI->nameBox->text().isEmpty() ) {
            avtUI->nameBox->QWidget::setFocus();
        }
        if( avtUI->raBox->text().isEmpty() ) {
            avtUI->raBox->QWidget::setFocus();
        } else {
            if ( avtUI->decBox->text().isEmpty() )
                avtUI->decBox->QWidget::setFocus();
        }
    }

    avtUI->View->update();
}

//Use find dialog to choose an object
void AltVsTime::slotBrowseObject() {
    QPointer<FindDialog> fd = new FindDialog(this);
    if ( fd->exec() == QDialog::Accepted ) {
        SkyObject *o = fd->selectedObject();
        processObject( o );
    }
    delete fd;

    avtUI->View->update();
}

void AltVsTime::processObject( SkyObject *o, bool forceAdd ) {
    if( !o )
        return;

    KSNumbers *num = new KSNumbers( getDate().djd() );
    KSNumbers *oldNum = 0;

    //If the object is in the solar system, recompute its position for the given epochLabel
    KStarsData* data = KStarsData::Instance();
    if ( o->isSolarSystem() ) {
        oldNum = new KSNumbers( data->ut().djd() );
        o->updateCoords( num, true, geo->lat(), data->lst() );
    }

    //precess coords to target epoch
    o->updateCoords( num );

    //If this point is not in list already, add it to list
    bool found(false);
    foreach ( SkyObject *p, pList ) {
        if ( o->ra().Degrees() == p->ra().Degrees() && o->dec().Degrees() == p->dec().Degrees() ) {
            found = true;
            break;
        }
    }
    if ( found && !forceAdd ) {
        qDebug() << "This point is already displayed; I will not duplicate it.";
    } else {
        pList.append( o );

        //make sure existing curves are thin and red
        foreach(KPlotObject* obj, avtUI->View->plotObjects()) {
            if ( obj->size() == 2 ) 
                obj->setLinePen( QPen( Qt::red, 1 ) );
        }

        //add new curve with width=2, and color=white
        KPlotObject *po = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
        for ( double h=-12.0; h<=12.0; h+=0.5 ) {
            int label_pos = -11.0 + avtUI->View->plotObjects().count();
            while ( label_pos > 11.0 )
                label_pos -= 23.0;
            if( h == label_pos )
                //po->addPoint( h, findAltitude( o, h ), o->translatedName() );
                po->addPoint( h, findAltitude( o, h ), getObjectName(o) );
            else
                po->addPoint( h, findAltitude( o, h ) );
        }
        avtUI->View->addPlotObject( po );

        //avtUI->PlotList->addItem( o->translatedName() );
        avtUI->PlotList->addItem( getObjectName(o) );
        avtUI->PlotList->setCurrentRow( avtUI->PlotList->count() - 1 );
        avtUI->raBox->showInHours(o->ra() );
        avtUI->decBox->showInDegrees(o->dec() );
        //avtUI->nameBox->setText(o->translatedName() );
        avtUI->nameBox->setText( getObjectName(o) );


        //Set epochName to epoch shown in date tab
        avtUI->epochName->setText( QString().setNum( getDate().epoch() ) );
    }
    qDebug() << "Currently, there are " << avtUI->View->plotObjects().count() << " objects displayed.";

    //restore original position
    if ( o->isSolarSystem() ) {
        o->updateCoords( oldNum, true, data->geo()->lat(), data->lst() );
        delete oldNum;
    }
    o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    delete num;
}

double AltVsTime::findAltitude( SkyPoint *p, double hour ) {
    hour += 24.0 * DayOffset;

    //getDate converts the user-entered local time to UT
    KStarsDateTime ut = getDate().addSecs( hour*3600.0 );

    dms LST = geo->GSTtoLST( ut.gst() );
    p->EquatorialToHorizontal( &LST, geo->lat() );
    return p->alt().Degrees();
}

void AltVsTime::slotHighlight( int row ) {
    //highlight the curve of the selected object
    QList< KPlotObject* > objects = avtUI->View->plotObjects();
    for ( int i=0; i<objects.count(); ++i ) {
        KPlotObject *obj = objects.at( i );

        if ( i == row ) {
            obj->setLinePen( QPen( Qt::white, 2 ) );
        } else {
            obj->setLinePen( QPen( Qt::red, 1 ) );
        }
    }

    avtUI->View->update();

    if( row >= 0 && row < pList.size() ) {
        SkyObject *p = pList.at(row);
        avtUI->raBox->showInHours( p->ra() );
        avtUI->decBox->showInDegrees( p->dec() );
        avtUI->nameBox->setText( avtUI->PlotList->currentItem()->text() );
    }
}

//move input focus to the next logical widget
void AltVsTime::slotAdvanceFocus() {
    if ( sender()->objectName() == QString( "nameBox" ) ) avtUI->addButton->setFocus();
    if ( sender()->objectName() == QString( "raBox" ) )   avtUI->decBox->setFocus();
    if ( sender()->objectName() == QString( "decbox" ) )  avtUI->addButton->setFocus();
    if ( sender()->objectName() == QString( "longBox" ) ) avtUI->latBox->setFocus();
    if ( sender()->objectName() == QString( "latBox" ) )  avtUI->updateButton->setFocus();
}

void AltVsTime::slotClear() {
    pList.clear();
    //Need to delete the pointers in deleteList
    while( !deleteList.isEmpty() )
        delete deleteList.takeFirst();

    avtUI->PlotList->clear();
    avtUI->nameBox->clear();
    avtUI->raBox->clear();
    avtUI->decBox->clear();
    avtUI->epochName->clear();
    avtUI->View->removeAllPlotObjects();
    avtUI->View->update();
}

void AltVsTime::slotClearBoxes() {
    avtUI->nameBox->clear();
    avtUI->raBox->clear() ;
    avtUI->decBox->clear();
    avtUI->epochName->clear();
}

void AltVsTime::computeSunRiseSetTimes() {
    //Determine the time of sunset and sunrise for the desired date and location
    //expressed as doubles, the fraction of a full day.
    KStarsDateTime today = getDate();
    KSAlmanac ksal;
    ksal.setDate( &today);
    ksal.setLocation(geo);
    double sunRise = ksal.getSunRise();
    double sunSet  = ksal.getSunSet();
    avtUI->View->setSunRiseSetTimes( sunRise, sunSet );
}

void AltVsTime::slotUpdateDateLoc() {
    KStarsData* data = KStarsData::Instance();
    KStarsDateTime today = getDate();
    KSNumbers *num = new KSNumbers( today.djd() );
    KSNumbers *oldNum = 0;
    dms LST = geo->GSTtoLST( today.gst() );

    //First determine time of sunset and sunrise
    computeSunRiseSetTimes();
    // Determine dawn/dusk time and min/max sun elevation
    setDawnDusk();

    for ( int i = 0; i < avtUI->PlotList->count(); ++i ) {
        QString oName = avtUI->PlotList->item( i )->text().toLower();

        SkyObject *o = data->objectNamed( oName );
        if ( o ) {
            //If the object is in the solar system, recompute its position for the given date
            if ( o->isSolarSystem() ) {
                oldNum = new KSNumbers( data->ut().djd() );
                o->updateCoords( num, true, geo->lat(), &LST );
            }

            //precess coords to target epoch
            o->updateCoords( num );

            //update pList entry
            pList.replace( i, o );

            KPlotObject *po = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
            for ( double h=-12.0; h<=12.0; h+=0.5 ) {
                po->addPoint( h, findAltitude( o, h ) );
            }
            avtUI->View->replacePlotObject( i, po );

            //restore original position
            if ( o->isSolarSystem() ) {
                o->updateCoords( oldNum, true, data->geo()->lat(), data->lst() );
                delete oldNum;
                oldNum = 0;
            }
            o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        } else {  //assume unfound object is a custom object
            pList.at(i)->updateCoords( num ); //precess to desired epoch

            KPlotObject *po = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
            for ( double h=-12.0; h<=12.0; h+=0.5 ) {
                po->addPoint( h, findAltitude( pList.at(i), h ) );
            }
            avtUI->View->replacePlotObject( i, po );
        }
    }

    if ( getDate().time().hour() > 12 )
        DayOffset = 1;
    else
        DayOffset = 0;

    setLSTLimits();
    slotHighlight( avtUI->PlotList->currentRow() );
    avtUI->View->update();

    delete num;
}

void AltVsTime::slotChooseCity() {
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if ( ld->exec() == QDialog::Accepted ) {
        GeoLocation *newGeo = ld->selectedCity();
        if ( newGeo ) {
            geo = newGeo;
            avtUI->latBox->showInDegrees( geo->lat() );
            avtUI->longBox->showInDegrees( geo->lng() );
        }
    }
    delete ld;
}

void AltVsTime::setLSTLimits() {
    //UT at noon on target date
    KStarsDateTime ut = getDate().addSecs( ((double)DayOffset + 0.5)*86400. );

    dms lst = geo->GSTtoLST( ut.gst() );
    double h1 = lst.Hours();
    if( h1 > 12.0 )
        h1 -= 24.0;
    double h2 = h1 + 24.0;
    avtUI->View->setSecondaryLimits( h1, h2, -90.0, 90.0 );
}

void AltVsTime::showCurrentDate()
{
    KStarsDateTime dt = KStarsDateTime::currentDateTime();
    if( dt.time() > QTime( 12, 0, 0 ) )
        dt = dt.addDays( 1 );
    avtUI->DateWidget->setDate( dt.date() );
}

KStarsDateTime AltVsTime::getDate()
{
    //convert midnight local time to UT:
    KStarsDateTime dt( avtUI->DateWidget->date(), QTime() );
    return geo->LTtoUT( dt );
}

double AltVsTime::getEpoch(const QString &eName)
{
    //If Epoch field not a double, assume J2000
    bool ok;
    double epoch = eName.toDouble(&ok);
    if ( !ok ) {
        qDebug() << "Invalid Epoch.  Assuming 2000.0.";
        return 2000.0;
    }
    return epoch;
}

void AltVsTime::setDawnDusk()
{
    KStarsDateTime today = getDate();
    KSNumbers num( today.djd() );
    dms LST = geo->GSTtoLST( today.gst() );

    KSSun sun;
    sun.updateCoords( &num, true, geo->lat(), &LST );
    double dawn, da, dusk, du, max_alt, min_alt;
    double last_h = -12.0;
    double last_alt = findAltitude( &sun, last_h );
    dawn = dusk = -13.0;
    max_alt = -100.0;
    min_alt = 100.0;
    for ( double h=-11.95; h<=12.0; h+=0.05 ) {
        double alt = findAltitude( &sun, h );
        bool   asc = alt - last_alt > 0;
        if ( alt > max_alt )
            max_alt = alt;
        if ( alt < min_alt )
            min_alt = alt;

        if ( asc && last_alt <= -18.0 && alt >= -18.0 )
            dawn = h;
        if ( !asc && last_alt >= -18.0 && alt <= -18.0 )
            dusk = h;

        last_h   = h;
        last_alt = alt;
    }

    if ( dawn < -12.0 || dusk < -12.0 ) {
        da = -1.0;
        du = -1.0;
    } else {
        da = dawn / 24.0;
        du = ( dusk + 24.0 ) / 24.0;
    }

    avtUI->View->setDawnDuskTimes( da, du );
    avtUI->View->setMinMaxSunAlt( min_alt, max_alt );
}

void AltVsTime::slotPrint()
{
    QPainter p;                 // Our painter object
    QPrinter printer;           // Our printer object
    QString str_legend;         // Text legend
    QString str_year;           // Calendar's year
    int text_height = 200;      // Height of legend text zone in points
    QSize plot_size;            // Initial plot widget size
    QFont plot_font;            // Initial plot widget font
    int plot_font_size;         // Initial plot widget font size

    // Set printer resolution to 300 dpi
    printer.setResolution( 300 );

    // Open print dialog
    QPointer<QPrintDialog> dialog( KdePrint::createPrintDialog( &printer, this ) );
    dialog->setWindowTitle( xi18n( "Print elevation vs time plot" ) );
    if ( dialog->exec() == QDialog::Accepted ) {
        // Change mouse cursor
        QApplication::setOverrideCursor( Qt::WaitCursor );

        // Save plot widget font
        plot_font = avtUI->View->font();
        // Save plot widget font size
        plot_font_size = plot_font.pointSize();
        // Save calendar widget size
        plot_size = avtUI->View->size();

        // Set text legend
        str_legend = xi18n( "Elevation vs. Time Plot" );
        str_legend += "\n";
        str_legend += geo->fullName();
        str_legend += " - ";
        str_legend += avtUI->DateWidget->date().toString( "dd/MM/yyyy" );

        // Create a rectangle for legend text zone
        QRect text_rect( 0, 0, printer.width(), text_height );

        // Increase plot widget font size so it looks good in 300 dpi
        plot_font.setPointSize( plot_font_size * 2.5 );
        avtUI->View->setFont( plot_font );
        // Increase plot widget size to fit the entire page
        avtUI->View->resize( printer.width(), printer.height() - text_height );

        // Create a pixmap and render plot widget into it
        QPixmap pixmap( avtUI->View->size() );
        avtUI->View->render( &pixmap );

        // Begin painting on printer
        p.begin( &printer );
        // Draw legend
        p.drawText( text_rect, Qt::AlignLeft, str_legend );
        // Draw plot widget
        p.drawPixmap( 0, text_height, pixmap );
        // Ending painting
        p.end();

        // Restore plot widget font size
        plot_font.setPointSize( plot_font_size );
        avtUI->View->setFont( plot_font );
        // Restore calendar widget size
        avtUI->View->resize( plot_size );

        // Restore mouse cursor
        QApplication::restoreOverrideCursor();
    }
    delete dialog;
}

QString AltVsTime::getObjectName(const SkyObject *o, bool translated)
{
    QString finalObjectName;
    if( o->name() == "star" )
    {
        StarObject *s = (StarObject *)o;

        // JM: Enable HD Index stars to be added to the observing list.
        if( s->getHDIndex() != 0 )
                finalObjectName = QString("HD %1" ).arg( QString::number( s->getHDIndex() ) );
    }
    else
         finalObjectName = translated ? o->translatedName() : o->name();

    return finalObjectName;

}


#include "altvstime.moc"
