/***************************************************************************
                          detaildialog.cpp  -  description
                             -------------------
    begin                : Sun May 5 2002
    copyright            : (C) 2002 by Jason Harris and Jasem Mutlaq
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "detaildialog.h"

#include <QLineEdit>
#include <QPixmap>
#include <QRegExp>
#include <QTextStream>
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidgetItem>

#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <ktoolinvocation.h>
#include <ktemporaryfile.h>
#include <kio/netaccess.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "geolocation.h"
#include "ksutils.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksmoon.h"
#include "thumbnailpicker.h"
#include "Options.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/indidevice.h"
#include "indi/indimenu.h"
#include "indi/devicemanager.h"
#include "indi/indistd.h"
#endif

#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/skymapcomposite.h"

DetailDialog::DetailDialog(SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, QWidget *parent ) :
    KPageDialog( parent ),
    selectedObject(o),
    Data(0), Pos(0), Links(0), Adv(0), Log(0)
{
    setFaceType( Tabbed );
    setBackgroundRole( QPalette::Base );

    titlePalette = palette();
    titlePalette.setColor( backgroundRole(), palette().color( QPalette::Active, QPalette::Highlight ) );
    titlePalette.setColor( foregroundRole(), palette().color( QPalette::Active, QPalette::HighlightedText ) );

    //Create thumbnail image
    Thumbnail = new QPixmap( 200, 200 );

    setCaption( i18n( "Object Details" ) );
    setButtons( KDialog::Close );

    createGeneralTab();
    createPositionTab( ut, geo );
    createLinksTab();
    createAdvancedTab();
    createLogTab();
}

DetailDialog::~DetailDialog() {
    delete Thumbnail;
}

void DetailDialog::createGeneralTab()
{
    Data = new DataWidget(this);
    addPage( Data, i18n("General") );

    Data->Names->setPalette( titlePalette );

    //Connections
    connect( Data->ObsListButton, SIGNAL( clicked() ), this, SLOT( addToObservingList() ) );
    connect( Data->CenterButton, SIGNAL( clicked() ), this, SLOT( centerMap() ) );
    #ifdef HAVE_INDI_H
    connect( Data->ScopeButton, SIGNAL( clicked() ), this, SLOT( centerTelescope() ) );
    #else
    Data->ScopeButton->setEnabled(false);
    #endif
    connect( Data->Image, SIGNAL( clicked() ), this, SLOT( updateThumbnail() ) );

    Data->IllumLabel->setVisible( false );

    //Show object thumbnail image
    showThumbnail();

    //Fill in the data fields
    //Contents depend on type of object
    StarObject *s = 0L;
    DeepSkyObject *dso = 0L;
    KSPlanetBase *ps = 0L;
    QString pname, oname, objecttyp, constellationname;

    switch ( selectedObject->type() ) {
    case SkyObject::STAR:
        s = (StarObject *)selectedObject;

        Data->Names->setText( s->longname() );

        if( s->getHDIndex() != 0 ) {
            if( !s->longname().isEmpty() )
                Data->Names->setText( s->longname() + QString( ", HD%1" ).arg( QString::number( s->getHDIndex() ) ) );
            else
                Data->Names->setText( QString( ", HD%1" ).arg( QString::number( s->getHDIndex() ) ) );
        }
        objecttyp = s->sptype() + ' ' + i18n("star");
        Data->Magnitude->setText( i18nc( "number in magnitudes", "%1 mag" ,
                                         KGlobal::locale()->formatNumber( s->mag(), 1 ) ) );  //show to tenths place

        //The thumbnail image is empty, and isn't clickable for stars
        //Also, don't show the border around the Image QFrame.
        Data->Image->setFrameStyle( QFrame::NoFrame );
        disconnect( Data->Image, SIGNAL( clicked() ), this, SLOT( updateThumbnail() ) );

        //distance
        if ( s->distance() > 2000. || s->distance() < 0. )  // parallax < 0.5 mas
            Data->Distance->setText( QString(i18nc("larger than 2000 parsecs", "> 2000 pc") ) );
        else if ( s->distance() > 50.0 ) //show to nearest integer
            Data->Distance->setText( i18nc( "number in parsecs", "%1 pc" ,
                                            KGlobal::locale()->formatNumber( s->distance(), 0 ) ) );
        else if ( s->distance() > 10.0 ) //show to tenths place
            Data->Distance->setText( i18nc( "number in parsecs", "%1 pc" ,
                                            KGlobal::locale()->formatNumber( s->distance(), 1 ) ) );
        else //show to hundredths place
            Data->Distance->setText( i18nc( "number in parsecs", "%1 pc" ,
                                            KGlobal::locale()->formatNumber( s->distance(), 2 ) ) );

        //Note multiplicity/variablility in angular size label
        Data->AngSizeLabel->setText( QString() );
        Data->AngSize->setText( QString() );
        Data->AngSizeLabel->setFont( Data->AngSize->font() );
        if ( s->isMultiple() && s->isVariable() ) {
            Data->AngSizeLabel->setText( i18nc( "the star is a multiple star", "multiple" ) + ',' );
            Data->AngSize->setText( i18nc( "the star is a variable star", "variable" ) );
        } else if ( s->isMultiple() )
            Data->AngSizeLabel->setText( i18nc( "the star is a multiple star", "multiple" ) );
        else if ( s->isVariable() )
            Data->AngSizeLabel->setText( i18nc( "the star is a variable star", "variable" ) );

        break; //end of stars case

    case SkyObject::ASTEROID:  //[fall through to planets]
    case SkyObject::COMET: //[fall through to planets]
    case SkyObject::MOON: //[fall through to planets]
    case SkyObject::PLANET:
        ps = (KSPlanetBase *)selectedObject;

        Data->Names->setText( ps->longname() );
        //Type is "G5 star" for Sun
        if ( ps->name() == "Sun" )
            objecttyp = i18n("G5 star");
        else if ( ps->name() == "Moon" )
            objecttyp = ps->translatedName();
        else if ( ps->name() == i18n("Pluto") || ps->name() == "Ceres" || ps->name() == "Eris" ) // TODO: Check if Ceres / Eris have translations and i18n() them
            objecttyp = i18n("Dwarf planet");
        else
            objecttyp = ps->typeName();

        //Magnitude: The moon displays illumination fraction instead
        if ( selectedObject->name() == "Moon" ) {
            Data->IllumLabel->setVisible( true );
            Data->Illumination->setText( QString("%1 %").arg( KGlobal::locale()->formatNumber( ((KSMoon *)selectedObject)->illum()*100., 0 ) ) );
        }

        Data->Magnitude->setText( i18nc( "number in magnitudes", "%1 mag" ,
                                         KGlobal::locale()->formatNumber( ps->mag(), 1 ) ) );  //show to tenths place
        //Distance from Earth.  The moon requires a unit conversion
        if ( ps->name() == "Moon" ) {
            Data->Distance->setText( i18nc("distance in kilometers", "%1 km",
                                           KGlobal::locale()->formatNumber( ps->rearth()*AU_KM ) ) );
        } else {
            Data->Distance->setText( i18nc("distance in Astronomical Units", "%1 AU",
                                           KGlobal::locale()->formatNumber( ps->rearth() ) ) );
        }

        //Angular size; moon and sun in arcmin, others in arcsec
        if ( ps->angSize() ) {
            if ( ps->name() == "Sun" || ps->name() == "Moon" )
                Data->AngSize->setText( i18nc("angular size in arcminutes", "%1 arcmin",
                                              KGlobal::locale()->formatNumber( ps->angSize() ) ) ); // Needn't be a plural form because sun / moon will never contract to 1 arcminute
            else
                Data->AngSize->setText( i18nc("angular size in arcseconds","%1 arcsec",
                                              KGlobal::locale()->formatNumber( ps->angSize()*60.0 ) ) ); 
        } else {
            Data->AngSize->setText( "--" );
        }

        break; //end of planets/comets/asteroids case

    default: //deep-sky objects
        dso = (DeepSkyObject *)selectedObject;

        //Show all names recorded for the object
        if ( ! dso->longname().isEmpty() && dso->longname() != dso->name() ) {
            pname = dso->translatedLongName();
            oname = dso->translatedName();
        } else {
            pname = dso->translatedName();
        }

        if ( ! dso->translatedName2().isEmpty() ) {
            if ( oname.isEmpty() ) oname = dso->translatedName2();
            else oname += ", " + dso->translatedName2();
        }

        if ( dso->ugc() != 0 ) {
            if ( ! oname.isEmpty() ) oname += ", ";
            oname += "UGC " + QString::number( dso->ugc() );
        }
        if ( dso->pgc() != 0 ) {
            if ( ! oname.isEmpty() ) oname += ", ";
            oname += "PGC " + QString::number( dso->pgc() );
        }

        if ( ! oname.isEmpty() ) pname += ", " + oname;
        Data->Names->setText( pname );

        objecttyp = dso->typeName();

        if ( dso->mag() > 90.0 )
            Data->Magnitude->setText( "--" );
        else
            Data->Magnitude->setText( i18nc( "number in magnitudes", "%1 mag" ,
                                             KGlobal::locale()->formatNumber( dso->mag(), 1 ) ) );  //show to tenths place

        //No distances at this point...
        Data->Distance->setText( "--" );

        //Only show decimal place for small angular sizes
        if ( dso->a() > 10.0 )
            Data->AngSize->setText( i18nc("angular size in arcminutes", "%1 arcmin",
                                          KGlobal::locale()->formatNumber(dso->a(), 0 ) ) );
        else if ( dso->a() )
            Data->AngSize->setText( i18nc("angular size in arcminutes", "%1 arcmin",
                                          KGlobal::locale()->formatNumber( dso->a(), 1 ) ) );
        else
            Data->AngSize->setText( "--" );

        break;
    }

    //Common to all types:
    if ( selectedObject->type() == SkyObject::CONSTELLATION )
        Data->ObjectTypeInConstellation->setText(
            KStarsData::Instance()->skyComposite()->getConstellationBoundary()->constellationName( selectedObject ) );
    else
        Data->ObjectTypeInConstellation->setText( 
            i18nc("%1 type of sky object (planet, asteroid etc), %2 name of a constellation", "%1 in %2", objecttyp,
                  KStarsData::Instance()->skyComposite()->getConstellationBoundary()->constellationName( selectedObject ) ) );
}

void DetailDialog::createPositionTab( const KStarsDateTime &ut, GeoLocation *geo ) {
    Pos = new PositionWidget(this);
    addPage( Pos,  i18n("Position") );

    Pos->CoordTitle->setPalette( titlePalette );
    Pos->RSTTitle->setPalette( titlePalette );

    //Coordinates Section:
    //Don't use KLocale::formatNumber() for the epoch string,
    //because we don't want a thousands-place separator!
    QString sEpoch = QString::number( ut.epoch(), 'f', 1 );
    //Replace the decimal point with localized decimal symbol
    sEpoch.replace( '.', KGlobal::locale()->decimalSymbol() );

    Pos->RALabel->setText( i18n( "RA (%1):", sEpoch ) );
    Pos->DecLabel->setText( i18n( "Dec (%1):", sEpoch ) );
    Pos->RA->setText( selectedObject->ra().toHMSString() );
    Pos->Dec->setText( selectedObject->dec().toDMSString() );
    Pos->Az->setText( selectedObject->az().toDMSString() );
    dms a;
    if( Options::useAltAz() )
        a = selectedObject->alt();
    else
        a = selectedObject->altRefracted();
    Pos->Alt->setText( a.toDMSString() );

    //Hour Angle can be negative, but dms HMS expressions cannot.
    //Here's a kludgy workaround:
    dms lst = geo->GSTtoLST( ut.gst() );
    dms ha( lst.Degrees() - selectedObject->ra().Degrees() );
    QChar sgn('+');
    if ( ha.Hours() > 12.0 ) {
        ha.setH( 24.0 - ha.Hours() );
        sgn = '-';
    }
    Pos->HA->setText( QString("%1%2").arg(sgn).arg( ha.toHMSString() ) );

    //Airmass is approximated as the secant of the zenith distance,
    //equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
    if ( selectedObject->alt().Degrees() > 0.0 )
        Pos->Airmass->setText( KGlobal::locale()->formatNumber(
                                   1./sin( selectedObject->alt().radians() ), 2 ) );
    else
        Pos->Airmass->setText( "--" );

    //Rise/Set/Transit Section:

    //Prepare time/position variables
    QTime rt = selectedObject->riseSetTime( ut, geo, true ); //true = use rise time
    dms raz = selectedObject->riseSetTimeAz( ut, geo, true ); //true = use rise time

    //If transit time is before rise time, use transit time for tomorrow
    QTime tt = selectedObject->transitTime( ut, geo );
    dms talt = selectedObject->transitAltitude( ut, geo );
    if ( tt < rt ) {
        tt = selectedObject->transitTime( ut.addDays( 1 ), geo );
        talt = selectedObject->transitAltitude( ut.addDays( 1 ), geo );
    }

    //If set time is before rise time, use set time for tomorrow
    QTime st = selectedObject->riseSetTime(  ut, geo, false ); //false = use set time
    dms saz = selectedObject->riseSetTimeAz( ut, geo, false ); //false = use set time
    if ( st < rt ) {
        st = selectedObject->riseSetTime( ut.addDays( 1 ), geo, false ); //false = use set time
        saz = selectedObject->riseSetTimeAz( ut.addDays( 1 ), geo, false ); //false = use set time
    }

    if ( rt.isValid() ) {
        Pos->TimeRise->setText( QString().sprintf( "%02d:%02d", rt.hour(), rt.minute() ) );
        Pos->TimeSet->setText( QString().sprintf( "%02d:%02d", st.hour(), st.minute() ) );
        Pos->AzRise->setText( raz.toDMSString() );
        Pos->AzSet->setText( saz.toDMSString() );
    } else {
        if ( selectedObject->alt().Degrees() > 0.0 ) {
            Pos->TimeRise->setText( i18n( "Circumpolar" ) );
            Pos->TimeSet->setText( i18n( "Circumpolar" ) );
        } else {
            Pos->TimeRise->setText( i18n( "Never rises" ) );
            Pos->TimeSet->setText( i18n( "Never rises" ) );
        }

        Pos->AzRise->setText( i18nc( "Not Applicable", "N/A" ) );
        Pos->AzSet->setText( i18nc( "Not Applicable", "N/A" ) );
    }

    Pos->TimeTransit->setText( QString().sprintf( "%02d:%02d", tt.hour(), tt.minute() ) );
    Pos->AltTransit->setText( talt.toDMSString() );

    // Restore the position and other time-dependent parameters
    selectedObject->recomputeCoords( ut, geo );

}

void DetailDialog::createLinksTab()
{
    // don't create a link tab for an unnamed star
    if (selectedObject->name() == QString("star"))
        return;

    Links = new LinksWidget( this );
    addPage( Links, i18n( "Links" ) );

    Links->InfoTitle->setPalette( titlePalette );
    Links->ImagesTitle->setPalette( titlePalette );

    foreach ( const QString &s, selectedObject->InfoTitle() )
        Links->InfoTitleList->addItem( i18nc( "Image/info menu item (should be translated)", s.toLocal8Bit() ) );

    //Links->InfoTitleList->setCurrentRow(0);

    foreach ( const QString &s, selectedObject->ImageTitle() )
        Links->ImageTitleList->addItem( i18nc( "Image/info menu item (should be translated)", s.toLocal8Bit() ) );

     updateButtons();

    // Signals/Slots
    connect( Links->ViewButton, SIGNAL(clicked()), this, SLOT( viewLink() ) );
    connect( Links->AddLinkButton, SIGNAL(clicked()), KStars::Instance()->map(), SLOT( addLink() ) );
    connect( Links->EditLinkButton, SIGNAL(clicked()), this, SLOT( editLinkDialog() ) );
    connect( Links->RemoveLinkButton, SIGNAL(clicked()), this, SLOT( removeLinkDialog() ) );

    // When an item is selected in info list, selected items are cleared image list.
    connect( Links->InfoTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT( setCurrentLink(QListWidgetItem*) ) );
    connect( Links->InfoTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), Links->ImageTitleList, SLOT( clearSelection() ) );

    // vice versa
    connect( Links->ImageTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT( setCurrentLink(QListWidgetItem*) ) );
    connect( Links->ImageTitleList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), Links->InfoTitleList, SLOT( clearSelection() ));

    connect( Links->InfoTitleList, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT( viewLink() ) );
    connect( Links->ImageTitleList, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT( viewLink() ) );

    connect( Links->InfoTitleList, SIGNAL(itemSelectionChanged ()), this, SLOT( updateButtons() ) );
    connect( Links->ImageTitleList, SIGNAL(itemSelectionChanged ()), this, SLOT( updateButtons() ));

    connect( KStars::Instance()->map(), SIGNAL(linkAdded()), this, SLOT( updateLists() ) );
}

void DetailDialog::createAdvancedTab()
{
    // Don't create an adv tab for an unnamed star or if advinterface file failed loading
    // We also don't need adv dialog for solar system objects.
    if (selectedObject->name() == QString("star") ||
            KStarsData::Instance()->ADVtreeList.isEmpty() ||
            selectedObject->type() == SkyObject::PLANET ||
            selectedObject->type() == SkyObject::COMET ||
            selectedObject->type() == SkyObject::ASTEROID )
        return;

    Adv = new DatabaseWidget( this );
    addPage( Adv,  i18n( "Advanced" ) );

    connect( Adv->ADVTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(viewADVData()));

    populateADVTree();
}

void DetailDialog::createLogTab()
{
    //Don't create a a log tab for an unnamed star
    if ( selectedObject->name() == QString("star") )
        return;

    // Log Tab
    Log = new LogWidget( this );
    addPage( Log,  i18n( "Log" ) );

    Log->LogTitle->setPalette( titlePalette );

    if ( selectedObject->userLog().isEmpty() )
        Log->UserLog->setText(i18n("Record here observation logs and/or data on %1.", selectedObject->translatedName()));
    else
        Log->UserLog->setText(selectedObject->userLog());

    //Automatically save the log contents when the widget loses focus
    connect( Log->UserLog, SIGNAL( focusOut() ), this, SLOT( saveLogData() ) );
}


void DetailDialog::setCurrentLink(QListWidgetItem *it)
{
    m_CurrentLink = it;
}

void DetailDialog::viewLink()
{
    QString URL;

    if (m_CurrentLink == NULL) return;

    if ( m_CurrentLink->listWidget() == Links->InfoTitleList ) {
        URL = QString( selectedObject->InfoList().at( Links->InfoTitleList->row(m_CurrentLink) ) );
    }
    else if ( m_CurrentLink->listWidget() == Links->ImageTitleList ) {
        URL = QString( selectedObject->ImageList().at( Links->ImageTitleList->row(m_CurrentLink) ) );
    }

    if ( !URL.isEmpty() )
        KToolInvocation::invokeBrowser(URL);
}

void DetailDialog::updateLists()
{
    Links->InfoTitleList->clear();
    Links->ImageTitleList->clear();

    foreach ( const QString &s, selectedObject->InfoTitle() )
        Links->InfoTitleList->addItem( s );

    foreach ( const QString &s, selectedObject->ImageTitle() )
        Links->ImageTitleList->addItem( s );

    updateButtons();
}

void DetailDialog::updateButtons()
{

    bool anyLink=false;
    if (!Links->InfoTitleList->selectedItems().isEmpty() || !Links->ImageTitleList->selectedItems().isEmpty())
        anyLink = true;

    // Buttons could be disabled if lists are initially empty, we enable and disable them here
    // depending on the current status of the list.
    Links->ViewButton->setEnabled(anyLink);
    Links->EditLinkButton->setEnabled(anyLink);
    Links->RemoveLinkButton->setEnabled(anyLink);
}

void DetailDialog::editLinkDialog()
{
    int type=0, row=0;
    QString search_line, replace_line, currentItemTitle, currentItemURL;

    if (m_CurrentLink == NULL) return;

    KDialog editDialog( this );
    editDialog.setCaption( i18n("Edit Link") );
    editDialog.setButtons( KDialog::Ok | KDialog::Cancel );
    QFrame editFrame( &editDialog );

    if ( m_CurrentLink->listWidget() == Links->InfoTitleList )
    {
        row = Links->InfoTitleList->row( m_CurrentLink );

        currentItemTitle = m_CurrentLink->text();
        currentItemURL = selectedObject->InfoTitle().at( row );
        search_line = selectedObject->name();
        search_line += ':';
        search_line += currentItemTitle;
        search_line += ':';
        search_line += currentItemURL;
        type       = 0;
    }
    else if ( m_CurrentLink->listWidget() == Links->ImageTitleList )
    {
        row = Links->ImageTitleList->row( m_CurrentLink );

        currentItemTitle = m_CurrentLink->text();
        currentItemURL = selectedObject->ImageTitle().at( row ); 
        search_line = selectedObject->name();
        search_line += ':';
        search_line += currentItemTitle;
        search_line += ':';
        search_line += currentItemURL;
        type 	   = 1;
    }
    else return;

    QLineEdit editNameField(&editFrame);
    editNameField.setObjectName("nameedit");
    editNameField.home(false);
    editNameField.setText(currentItemTitle);
    QLabel editLinkURL(i18n("URL:"), &editFrame);
    QLineEdit editLinkField(&editFrame);
    editLinkField.setObjectName("urledit");
    editLinkField.home(false);
    editLinkField.setText(currentItemURL);
    QVBoxLayout vlay(&editFrame);
    vlay.setObjectName("vlay");
    QHBoxLayout editLinkLayout(&editFrame);
    editLinkLayout.setObjectName("editlinklayout");
    editLinkLayout.addWidget(&editLinkURL);
    editLinkLayout.addWidget(&editLinkField);
    vlay.addWidget( &editNameField );
    vlay.addLayout( &editLinkLayout );
    editDialog.setMainWidget( &editFrame );

    bool go( true );
    // If user presses cancel then skip the action
    if ( editDialog.exec() != QDialog::Accepted )
        go = false;

    // If nothing changed, skip th action
    if (editLinkField.text() == currentItemURL && editNameField.text() == currentItemTitle)
        go = false;

    if ( go ) {
        replace_line = selectedObject->name() + ':' + editNameField.text() + ':' + editLinkField.text();

        // Info Link
        if (type==0) {
            selectedObject->InfoTitle().replace(row, editNameField.text());
            selectedObject->InfoList().replace(row, editLinkField.text());

        // Image Links
        } else {
            selectedObject->ImageTitle().replace(row, editNameField.text());
            selectedObject->ImageList().replace(row, editLinkField.text());
        }

        // Update local files
        updateLocalDatabase(type, search_line, replace_line);

        // Set focus to the same item again
        if (type == 0)
            Links->InfoTitleList->setCurrentRow(row);
        else
            Links->ImageTitleList->setCurrentRow(row);
    }
}

void DetailDialog::removeLinkDialog()
{
    int type=0, row=0;
    QString currentItemURL, currentItemTitle, LineEntry, TempFileName, FileLine;
    QFile URLFile;
    KTemporaryFile TempFile;
    TempFile.setAutoRemove(false);
    TempFile.open();
    TempFileName = TempFile.fileName();

    if (m_CurrentLink == NULL) return;

    if ( m_CurrentLink->listWidget() == Links->InfoTitleList )
    {
        row = Links->InfoTitleList->row( m_CurrentLink );
        currentItemTitle = m_CurrentLink->text();
        currentItemURL   = selectedObject->InfoList()[row];
        LineEntry = selectedObject->name();
        LineEntry += ':';
        LineEntry += currentItemTitle;
        LineEntry += ':';
        LineEntry += currentItemURL;
        type       = 0;
    }
    else if ( m_CurrentLink->listWidget() == Links->ImageTitleList )
    {
        row = Links->ImageTitleList->row( m_CurrentLink );
        currentItemTitle = m_CurrentLink->text();
        currentItemURL   = selectedObject->ImageList()[row];
        LineEntry = selectedObject->name();
        LineEntry += ':';
        LineEntry += currentItemTitle;
        LineEntry += ':';
        LineEntry += currentItemURL;
        type 	   = 1;
    }
    else return;

    if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove the %1 link?", currentItemTitle), i18n("Delete Confirmation"),KStandardGuiItem::del())!=KMessageBox::Continue)
        return;

    if (type ==0)
    {
        selectedObject->InfoTitle().removeAt(row);
        selectedObject->InfoList().removeAt(row);
    }
    else
    {
        selectedObject->ImageTitle().removeAt(row);
        selectedObject->ImageList().removeAt(row);
    }

    // Remove link from file
    updateLocalDatabase(type, LineEntry);

    // Set focus to the 1st item in the list
    if (type == 0)
        Links->InfoTitleList->clearSelection();
    else
        Links->ImageTitleList->clearSelection();

}

void DetailDialog::updateLocalDatabase(int type, const QString &search_line, const QString &replace_line)
{
    QString TempFileName, file_line;
    QFile URLFile;
    KTemporaryFile TempFile;
    TempFile.setAutoRemove(false);
    TempFile.open();
    QTextStream *temp_stream=NULL, *out_stream=NULL;
    bool replace = !replace_line.isEmpty();

    if (search_line.isEmpty())
        return;

    TempFileName = TempFile.fileName();

    switch (type)
    {
        // Info Links
    case 0:
        // Get name for our local info_url file
        URLFile.setFileName( KStandardDirs::locateLocal( "appdata", "info_url.dat" ) );
        break;

        // Image Links
    case 1:
        // Get name for our local info_url file
        URLFile.setFileName( KStandardDirs::locateLocal( "appdata", "image_url.dat" ) );
        break;
    }

    // Copy URL file to temp file
    KIO::file_copy(KUrl::fromPath(URLFile.fileName()), KUrl::fromPath(TempFileName), -1, KIO::Overwrite | KIO::HideProgressInfo );


    if ( !URLFile.open( QIODevice::WriteOnly) )
    {
        kDebug() << "DetailDialog: Failed to open " << URLFile.fileName();
        kDebug() << "KStars cannot save to user database";
        return;
    }

    // Get streams;
    temp_stream = new QTextStream(&TempFile);
    out_stream  = new QTextStream(&URLFile);

    while (!temp_stream->atEnd())
    {
        file_line = temp_stream->readLine();
        // If we find a match, either replace, or remove (by skipping).
        if (file_line == search_line)
        {
            if (replace)
                (*out_stream) << replace_line << endl;
            else
                continue;
        }
        else
            (*out_stream) << file_line << endl;
    }

    URLFile.close();
    delete(temp_stream);
    delete(out_stream);

    updateLists();

}

void DetailDialog::populateADVTree()
{
    QTreeWidgetItem *parent = NULL, *temp = NULL;

    // We populate the tree iterativley, keeping track of parents as we go
    // This solution is more efficient than the previous recursion algorithm.
    foreach (ADVTreeData *item, KStarsData::Instance()->ADVtreeList)
    {

        switch (item->Type)
        {
            // Top Level
        case 0:
            temp = new QTreeWidgetItem(parent, QStringList(item->Name));
            if (parent == NULL)
                Adv->ADVTree->addTopLevelItem(temp);
            parent = temp;

            break;

            // End of top level
        case 1:
            if (parent != NULL) parent = parent->parent();
            break;

            // Leaf
        case 2:
            new QTreeWidgetItem(parent, QStringList(item->Name));
            break;
        }
    }

}

void  DetailDialog::viewADVData()
{
    QString link;
    QTreeWidgetItem * current = Adv->ADVTree->currentItem();

    //If the item has children or is invalid, do nothing
    if ( !current || current->childCount()>0 )  return;

    foreach (ADVTreeData *item, KStarsData::Instance()->ADVtreeList)
    {
        if (item->Name == current->text(0))
        {
            link = item->Link;
            link = parseADVData(link);
            KToolInvocation::invokeBrowser(link);
            return;
        }
    }
}

QString DetailDialog::parseADVData( const QString &inlink )
{
    QString link = inlink;
    QString subLink;
    int index;

    if ( (index = link.indexOf("KSOBJ")) != -1)
    {
        link.remove(index, 5);
        link = link.insert(index, selectedObject->name());
    }

    if ( (index = link.indexOf("KSRA")) != -1)
    {
        link.remove(index, 4);
        subLink.sprintf("%02d%02d%02d", selectedObject->ra0().hour(), selectedObject->ra0().minute(), selectedObject->ra0().second());
        subLink = subLink.insert(2, "%20");
        subLink = subLink.insert(7, "%20");

        link = link.insert(index, subLink);
    }
    if ( (index = link.indexOf("KSDEC")) != -1)
    {
        link.remove(index, 5);
        if (selectedObject->dec().degree() < 0)
        {
            subLink.sprintf("%03d%02d%02d", selectedObject->dec0().degree(), selectedObject->dec0().arcmin(), selectedObject->dec0().arcsec());
            subLink = subLink.insert(3, "%20");
            subLink = subLink.insert(8, "%20");
        }
        else
        {
            subLink.sprintf("%02d%02d%02d", selectedObject->dec0().degree(), selectedObject->dec0().arcmin(), selectedObject->dec0().arcsec());
            subLink = subLink.insert(0, "%2B");
            subLink = subLink.insert(5, "%20");
            subLink = subLink.insert(10, "%20");
        }
        link = link.insert(index, subLink);
    }

    return link;
}

void DetailDialog::saveLogData() {
    selectedObject->saveUserLog( Log->UserLog->toPlainText() );
}

void DetailDialog::addToObservingList() {
    KStars::Instance()->observingList()->slotAddObject( selectedObject );
}

void DetailDialog::centerMap() {
    SkyMap* map = KStars::Instance()->map();
    map->setClickedObject( selectedObject );
    map->slotCenter();
}

void DetailDialog::centerTelescope()
{
#ifdef HAVE_INDI_H

    INDI_D *indidev(NULL);
    INDI_E *ConnectEle(NULL);

    // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
    // i.e. the first telescope we find!
    INDIMenu *imenu = KStars::Instance()->indiMenu();

    for (int i=0; i < imenu->managers.size() ; i++)
    {
        for (int j=0; j < imenu->managers.at(i)->indi_dev.size(); j++)
        {
            indidev = imenu->managers.at(i)->indi_dev.at(j);
            ConnectEle = indidev->findElem("CONNECT");
            if (!ConnectEle) continue;

            if (ConnectEle->state == PS_OFF)
            {
                KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", indidev->label));
                return;
            }

	    if (!indidev->stdDev->slew_scope(static_cast<SkyPoint *> (selectedObject)))
		continue;

	    return;
        }
    }

    // We didn't find any telescopes
    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
#endif
}

void DetailDialog::showThumbnail() {
    //No image if object is a star
    if ( selectedObject->type() == SkyObject::STAR ||
         selectedObject->type() == SkyObject::CATALOG_STAR ) {
        Thumbnail->scaled( Data->Image->width(), Data->Image->height() );
        Thumbnail->fill( Data->DataFrame->palette().color( QPalette::Window ) );
        Data->Image->setPixmap( *Thumbnail );
        return;
    }

    //Try to load the object's image from disk
    //If no image found, load "no image" image
    //If that isn't found, make it blank.
    QFile file;
    QString fname = "thumb-" + selectedObject->name().toLower().remove( ' ' ) + ".png";
    if ( KSUtils::openDataFile( file, fname ) ) {
        file.close();
        Thumbnail->load( file.fileName(), "PNG" );
    } else if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
        file.close();
        Thumbnail->load( file.fileName(), "PNG" );
    } else {
        Thumbnail->scaled( Data->Image->width(), Data->Image->height() );
        Thumbnail->fill( Data->DataFrame->palette().color( QPalette::Window ) );
    }

    Data->Image->setPixmap( *Thumbnail );
}

void DetailDialog::updateThumbnail() {
    QPointer<ThumbnailPicker> tp = new ThumbnailPicker( selectedObject, *Thumbnail, this );

    if ( tp->exec() == QDialog::Accepted ) {
        QString fname = KStandardDirs::locateLocal( "appdata", "thumb-" + selectedObject->name().toLower().remove( ' ' ) + ".png" );

        Data->Image->setPixmap( *(tp->image()) );

        //If a real image was set, save it.
        //If the image was unset, delete the old image on disk.
        if ( tp->imageFound() ) {
            Data->Image->pixmap()->save( fname, "PNG" );
            *Thumbnail = *(Data->Image->pixmap());
        } else {
            QFile f;
            f.setFileName( fname );
            f.remove();
        }
    }
    delete tp;
}

DataWidget::DataWidget( QWidget *p ) : QFrame( p )
{
    setupUi( this );
    DataFrame->setBackgroundRole( QPalette::Base );
}

PositionWidget::PositionWidget( QWidget *p ) : QFrame( p )
{
    setupUi( this );
    CoordFrame->setBackgroundRole( QPalette::Base );
    RSTFrame->setBackgroundRole( QPalette::Base );
}

LinksWidget::LinksWidget( QWidget *p ) : QFrame( p )
{
    setupUi( this );
}

DatabaseWidget::DatabaseWidget( QWidget *p ) : QFrame( p )
{
    setupUi( this );
}

LogWidget::LogWidget( QWidget *p ) : QFrame( p )
{
    setupUi( this );
}

#include "detaildialog.moc"
