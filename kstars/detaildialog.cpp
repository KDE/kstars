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

#include <qlineedit.h>
#include <qtextedit.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qstring.h>
#include <qlayout.h>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <klistview.h>

#include "detaildialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "geolocation.h"
#include "ksutils.h"
#include "skymap.h"
#include "skyobject.h"
#include "starobject.h"
#include "deepskyobject.h"
#include "ksplanetbase.h"
#include "ksmoon.h"

LogEdit::LogEdit( QWidget *parent, const char *name ) : KTextEdit( parent, name ) 
{
}

void LogEdit::focusOutEvent( QFocusEvent *e ) {
	emit focusOut();
	QWidget::focusOutEvent(e);
}

DetailDialog::DetailDialog(SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, 
		QWidget *parent, const char *name ) : 
		KDialogBase( KDialogBase::Tabbed, i18n( "Object Details" ), Close, Close, parent, name ) ,
		selectedObject(o), ksw((KStars*)parent)
{
	createGeneralTab( ut, geo );
	createLinksTab();
	createAdvancedTab();
	createLogTab();
}

void DetailDialog::createLogTab()
{
	// We don't create a a log tab for an unnamed object
	if (selectedObject->name() == QString("star"))
			return;

	// Log Tab
	logTab = addPage(i18n("Log"));

	userLog = new LogEdit(logTab, "userLog");
//	userLog->setTextFormat(Qt::RichText);

	if (selectedObject->userLog.isEmpty())
		userLog->setText(i18n("Record here observation logs and/or data on %1.").arg(selectedObject->translatedName()));
	else
		userLog->setText(selectedObject->userLog);

	logLayout = new QVBoxLayout(logTab, 6, 6, "logLayout");
	logLayout->addWidget(userLog);

	connect( userLog, SIGNAL( focusOut() ), this, SLOT( saveLogData() ) );
}

void DetailDialog::createAdvancedTab()
{
	// We don't create an adv tab for an unnamed object or if advinterface file failed loading
	// We also don't need adv dialog for solar system objects.
   if (selectedObject->name() == QString("star") || ksw->data()->ADVtreeList.isEmpty() || 
				selectedObject->type() == SkyObject::PLANET || 
				selectedObject->type() == SkyObject::COMET || 
				selectedObject->type() == SkyObject::ASTEROID )
		return;

	advancedTab = addPage(i18n("Advanced"));

	ADVTree = new KListView(advancedTab, "advancedtree");
	ADVTree->addColumn(i18n("Data"));
	ADVTree->setRootIsDecorated(true);

	viewTreeItem = new QPushButton (i18n("View"), advancedTab, "view");

	ADVbuttonSpacer = new QSpacerItem(40, 10, QSizePolicy::Expanding, QSizePolicy::Minimum);

	ADVbuttonLayout = new QHBoxLayout(5, "buttonlayout");
	ADVbuttonLayout->addWidget(viewTreeItem);
	ADVbuttonLayout->addItem(ADVbuttonSpacer);

	treeLayout = new QVBoxLayout(advancedTab, 6, 6, "treeLayout");
	treeLayout->addWidget(ADVTree);
	treeLayout->addLayout(ADVbuttonLayout);

	treeIt = new QPtrListIterator<ADVTreeData> (ksw->data()->ADVtreeList);

	connect(viewTreeItem, SIGNAL(clicked()), this, SLOT(viewADVData()));
	connect(ADVTree, SIGNAL(doubleClicked(QListViewItem*)), this, SLOT(viewADVData()));

	populateADVTree(NULL);

	//ADVtreeRoot->setOpen(true);
}


void DetailDialog::createLinksTab()
{
	// We don't create a link tab for an unnamed object
	if (selectedObject->name() == QString("star"))
		return;

	linksTab = addPage(i18n("Links"));

	infoBox = new QGroupBox(i18n("Info Links"), linksTab, "linksgroup");
	infoLayout = new QVBoxLayout(infoBox, 20, 0, "linksbox");
	infoList = new KListBox(infoBox, "links");
	infoLayout->addWidget(infoList);

	imagesBox = new QGroupBox(i18n("Image Links"), linksTab, "imagesgroup");
	imagesLayout = new QVBoxLayout(imagesBox, 20, 0, "imagesbox");
	imagesList = new KListBox(imagesBox, "links");
	imagesLayout->addWidget(imagesList);

	view = new QPushButton(i18n("View"), linksTab, "view");
	addLink = new QPushButton(i18n("Add Link..."), linksTab, "addlink");
	editLink = new QPushButton(i18n("Edit Link..."), linksTab, "editlink");
	removeLink = new QPushButton(i18n("Remove Link"), linksTab, "removelink");
	buttonSpacer = new QSpacerItem(40, 10, QSizePolicy::Expanding, QSizePolicy::Minimum);

	buttonLayout = new QHBoxLayout(5, "buttonlayout");
	buttonLayout->addWidget(view);
	buttonLayout->addWidget(addLink);
	buttonLayout->addWidget(editLink);
	buttonLayout->addWidget(removeLink);
	buttonLayout->addItem(buttonSpacer);

	topLayout = new QVBoxLayout(linksTab, 6, 6 , "toplayout");
	topLayout->addWidget(infoBox);
	topLayout->addWidget(imagesBox);
	topLayout->addLayout(buttonLayout);

	QStringList::Iterator itList = selectedObject->InfoList.begin();
	QStringList::Iterator itTitle = selectedObject->InfoTitle.begin();
	QStringList::Iterator itListEnd = selectedObject->InfoList.end();

	for ( ; itList != itListEnd; ++itList ) {
		infoList->insertItem(QString(*itTitle));
		itTitle++;
	}

	infoList->setSelected(0, true);

	itList  = selectedObject->ImageList.begin();
	itTitle = selectedObject->ImageTitle.begin();
	itListEnd  = selectedObject->ImageList.end();

	for ( ; itList != itListEnd; ++itList ) {
		imagesList->insertItem(QString(*itTitle));
		itTitle++;
	}

	if (!infoList->count() && !imagesList->count())
			editLink->setDisabled(true);

	// Signals/Slots
	connect(view, SIGNAL(clicked()), this, SLOT(viewLink()));
	connect(addLink, SIGNAL(clicked()), ksw->map(), SLOT(addLink()));
	connect(ksw->map(), SIGNAL(linkAdded()), this, SLOT(updateLists()));
	connect(editLink, SIGNAL(clicked()), this, SLOT(editLinkDialog()));
	connect(removeLink, SIGNAL(clicked()), this, SLOT(removeLinkDialog()));
	connect(infoList, SIGNAL(highlighted(int)), this, SLOT(unselectImagesList()));
	connect(imagesList, SIGNAL(highlighted(int)), this, SLOT(unselectInfoList()));
}

void DetailDialog::createGeneralTab( const KStarsDateTime &ut, GeoLocation *geo )
{
	QFrame *generalTab= addPage(i18n("General"));

	dms lst = ksw->data()->geo()->GSTtoLST( ut.gst() );
	
	Coords = new CoordBox( selectedObject, ut.epoch(), &lst, generalTab );
	RiseSet = new RiseSetBox( selectedObject, ut, geo, generalTab );

	StarObject *s;
	DeepSkyObject *dso;
	KSPlanetBase *ps;
	
	QString pname, oname, distStr, angStr, illumStr, magStr;
	QString sflags( "" );
	
//arguments to NameBox depend on type of object
	switch ( selectedObject->type() ) {
	case 0: //stars
		s = (StarObject *)selectedObject;
		pname = s->translatedName();
		if ( pname == i18n( "star" ) ) pname = i18n( "Unnamed star" );
		
		//distance
		if ( s->distance() > 50.0 ) //show to nearest integer
			distStr = QString::number( int( s->distance() + 0.5 ) ) + i18n(" parsecs", " pc");
		else if ( s->distance() > 10.0 ) //show to tenths place
			distStr = KGlobal::locale()->formatNumber( s->distance(), 1 ) + i18n(" parsecs", " pc");
		else //show to hundredths place
			distStr = KGlobal::locale()->formatNumber( s->distance() ) + i18n(" parsecs", " pc");
		// astrometric precision limit for Hipparcos is:
		// This is not clear:
		// 7 mas if V < 9   => 7 mas -> 142 pc 
		// 25 mas if V > 10 => 25 mas -> 40 pc

		/*
		double magnitude = 10;
		magnitude = mag.toDouble();

		if (distance > 142) 
			distStr = QString(i18n("larger than 142 parsecs", "> 142 pc") );
		if (magnitude >= 10 && distance > 40 )
			distStr = QString( i18n("larger than 40 parsecs", " > 40 pc") );
		*/
		if (s->distance() > 2000 || s->distance() < 0)  // parallax < 0.5 mas 
			distStr = QString(i18n("larger than 2000 parsecs", "> 2000 pc") );

		if ( s->isMultiple() ) sflags += i18n( "the star is a multiple star", "multiple" );
		if ( s->isMultiple() && s->isVariable() ) sflags += ", ";
		if ( s->isVariable() ) sflags += i18n( "the star is a variable star", "variable" );
		
		Names = new NameBox( pname, s->gname(),
				i18n( "Spectral type:" ), s->sptype(),
				QString("%1").arg( s->mag() ), distStr, sflags, generalTab, 0, false );
//		ProperMotion = new ProperMotionBox( s );
		break;
	case 2: //planets
		//Want to add distance from Earth, Mass, angular size.
		//Perhaps: list of moons

		ps = (KSPlanetBase *)selectedObject;
		
		//Construct string for distance from Earth.  The moon requires a unit conversion
		if ( ps->name() == "Moon" ) {
			distStr = i18n("distance in kilometers", "%1 km").arg( KGlobal::locale()->formatNumber( ps->rearth()*AU_KM ) );
		} else {
			distStr = i18n("distance in Astronomical Units", "%1 AU").arg( KGlobal::locale()->formatNumber( ps->rearth() ) );
		}

		// Construct string for magnitude:
		magStr = KGlobal::locale()->formatNumber( ps->mag(), 1 );
		
		//Construct the string for angular size
		angStr = "--";
		if ( ps->angSize() ) {
			angStr = i18n("angular size in arcseconds", "%1 arcsec").arg( KGlobal::locale()->formatNumber( ps->angSize()*60.0 ) );
			if ( ps->name() == "Sun" || ps->name() == "Moon" ) 
				angStr = i18n("angular size in arcminutes", "%1 arcmin").arg( KGlobal::locale()->formatNumber( ps->angSize() ) );
		} 
		
		//the Sun should display type=star, not planet!
		if ( selectedObject->name() == "Sun" ) {
			Names = new NameBox( selectedObject->translatedName(), "", i18n( "Object type:" ),
													 i18n("star"), KGlobal::locale()->formatNumber( -26.8 ), distStr, angStr, generalTab );
		
		//the Moon displays illumination fraction instead of magnitude
		} else if ( selectedObject->name() == "Moon" ) {
			//special string that will signal NameBox to use label "Illumination" instead of "Magnitude"
			//I can't seem to recats selectedObject as a KSMoon, so I'll just grab the pointer from KStarsData
			illumStr = QString("i%1 %").arg( int( ((KSMoon *)selectedObject)->illum()*100. ) );
			Names = new NameBox( selectedObject->translatedName(), ((KSMoon *)selectedObject)->phaseName(), i18n( "Object type:" ),
					selectedObject->typeName(), illumStr, distStr, angStr, generalTab );
		
		} else {
			Names = new NameBox( selectedObject->translatedName(), "", i18n( "Object type:" ),
					selectedObject->typeName(), magStr, distStr, angStr, generalTab );
		}
		break;
	case 9:  //comets
	case 10: //asteroids:
		ps = (KSPlanetBase *)selectedObject;
		distStr = i18n("distance in Astronomical Units", "%1 AU").arg( KGlobal::locale()->formatNumber( ps->rearth() ) );
		Names = new NameBox( selectedObject->translatedName(), "", i18n( "Object type:" ),
					selectedObject->typeName(), "--", distStr, "--", generalTab );
		break;
	default: //deep-sky objects
		dso = (DeepSkyObject *)selectedObject;

		if ( ! dso->longname().isEmpty() && dso->longname() != dso->name() ) {
			pname = dso->translatedLongName();
			oname = dso->translatedName();
		} else {
			pname = dso->translatedName();
		}

		if ( ! dso->name2().isEmpty() ) {
			if ( oname.isEmpty() ) oname = dso->translatedName2();
			else oname += ", " + dso->translatedName2();
		}

		if ( dso->ugc() != 0 ) oname += ", UGC " + QString("%1").arg( dso->ugc() );
		if ( dso->pgc() != 0 ) oname += ", PGC " + QString("%1").arg( dso->pgc() );

		//Only show decimal place for small angular sizes
		if ( dso->a() > 10.0 ) 
			angStr = i18n("angular size in arcminutes", "%1 arcmin").arg( int( dso->a() ) );
		else if ( dso->a() ) 
			angStr = i18n("angular size in arcminutes", "%1 arcmin").arg( KGlobal::locale()->formatNumber( dso->a(), 1 ) );
		else 
			angStr = "--";
		
		Names = new NameBox( pname, oname, i18n( "Object type:" ),
				dso->typeName(), QString("%1").arg( dso->mag() ), "--", angStr, generalTab );
		break;
	}

//Layout manager
	vlay = new QVBoxLayout( generalTab, 2 );
	vlay->addWidget( Names );
	vlay->addWidget( Coords );
	vlay->addWidget( RiseSet );
	vlay->activate();

}

DetailDialog::NameBox::NameBox( QString pname, QString oname,
		QString typelabel, QString type, QString mag,
		QString distStr, QString size, QWidget *parent, const char *name, bool useSize )
		: QGroupBox( i18n( "General" ), parent, name ) {

	PrimaryName = new QLabel( pname, this );
	OtherNames = new QLabel( oname, this );

	TypeLabel = new QLabel( typelabel, this );
	Type = new QLabel( type, this );
	Type->setAlignment( AlignRight );
	QFont boldFont = Type->font();
	boldFont.setWeight( QFont::Bold );
	PrimaryName->setFont( boldFont );
	Type->setFont( boldFont );
	
	MagLabel = new QLabel( i18n( "Magnitude:" ), this );
	if ( mag.startsWith( "i" ) ) {
		MagLabel->setText( i18n( "Illumination:" ) );
		mag = mag.mid(1);
	} 
	
	Mag = new QLabel( mag, this );
	if ( mag == "99.9" ) {
		Mag->setText( "--" );
	}
	
	Mag->setAlignment( AlignRight );
	Mag->setFont( boldFont );

	DistLabel = new QLabel( i18n( "Distance:" ), this);
	Dist = new QLabel (distStr, this);
	Dist->setAlignment( AlignRight );
	Dist->setFont( boldFont );

	if ( useSize ) { SizeLabel = new QLabel( i18n( "Angular size:" ), this ); }
	AngSize = new QLabel( size, this );
	AngSize->setAlignment( AlignRight );
	AngSize->setFont( boldFont );

//Layout
	hlayType = new QHBoxLayout( 2 );
	hlayMag  = new QHBoxLayout( 2 );
	hlayDist = new QHBoxLayout( 2 );
	hlaySize = new QHBoxLayout( 2 );
	glay     = new QGridLayout( 2, 4, 10 );
	vlay     = new QVBoxLayout( this, 12 );

	hlayType->addWidget( TypeLabel );
	hlayType->addWidget( Type );
	hlayMag->addWidget( MagLabel );
	hlayMag->addWidget( Mag );
	hlayDist->addWidget( DistLabel);
	hlayDist->addWidget( Dist);
	if ( useSize ) hlaySize->addWidget( SizeLabel );
	hlaySize->addWidget( AngSize );

	glay->setColStretch( 0, 1 );

	glay->addWidget( PrimaryName, 0, 0);
	glay->addWidget( OtherNames, 1, 0);
	glay->addLayout( hlayType, 0, 1 );
	glay->addLayout( hlayMag, 1, 1 );
	glay->addLayout( hlayDist, 2, 1 );
	glay->addLayout( hlaySize, 3, 1 );
	vlay->addSpacing( 10 );
	vlay->addLayout( glay );
}

//In the printf() statements, the "176" refers to the ASCII code for the degree symbol

DetailDialog::CoordBox::CoordBox( SkyObject *o, double epoch, dms *LST, QWidget *parent,
		const char *name ) : QGroupBox( i18n( "Coordinates" ), parent, name ) {

			RALabel = new QLabel( i18n( "RA (%1):" ).arg( KGlobal::locale()->formatNumber( epoch ) ), this );
			DecLabel = new QLabel( i18n( "Dec (%1):" ).arg( KGlobal::locale()->formatNumber( epoch ) ), this );
	HALabel = new QLabel( i18n( "Hour angle:" ), this );
	
	RA  = new QLabel( o->ra()->toHMSString(), this );
	Dec = new QLabel( o->dec()->toDMSString(), this );
	dms ha( LST->Degrees() - o->ra()->Degrees() );
	
	//Hour Angle can be negative, but dms HMS expressions cannot.  
	//Here's a kludgy workaround:
	QChar sgn('+');
	if ( ha.Hours() > 12.0 ) {
		ha.setH( 24.0 - ha.Hours() );
		sgn = '-';
	}
	HA = new QLabel( QString("%1%2").arg(sgn).arg(ha.toHMSString()), this );
	
	AzLabel = new QLabel( i18n( "Azimuth:" ), this );
	AltLabel = new QLabel( i18n( "Altitude:" ), this );
	AirMassLabel = new QLabel( i18n( "Airmass:" ), this );
	
	Az = new QLabel( o->az()->toDMSString(), this );
	Alt = new QLabel( o->alt()->toDMSString(), this );
	
	//airmass is approximated as the secant of the zenith distance,
	//equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
	if ( o->alt()->Degrees() > 0.0 ) 
		AirMass = new QLabel( QString("%1").arg( KGlobal::locale()->formatNumber( 1./sin( o->alt()->radians() ) ) ), this );
	else 
		AirMass = new QLabel( "--", this );

	QFont boldFont = RA->font();
	boldFont.setWeight( QFont::Bold );
	RA->setFont( boldFont );
	Dec->setFont( boldFont );
	HA->setFont( boldFont );
	Az->setFont( boldFont );
	Alt->setFont( boldFont );
	AirMass->setFont( boldFont );
	RA->setAlignment( AlignRight );
	Dec->setAlignment( AlignRight );
	HA->setAlignment( AlignRight );
	Az->setAlignment( AlignRight );
	Alt->setAlignment( AlignRight );
	AirMass->setAlignment( AlignRight );

//Layouts
	glayCoords = new QGridLayout( 5, 3, 10 );
	vlayMain = new QVBoxLayout( this, 12 );

	glayCoords->addWidget( RALabel, 0, 0 );
	glayCoords->addWidget( DecLabel, 1, 0 );
	glayCoords->addWidget( HALabel, 2, 0 );
	glayCoords->addWidget( RA, 0, 1 );
	glayCoords->addWidget( Dec, 1, 1 );
	glayCoords->addWidget( HA, 2, 1 );
	glayCoords->addItem( new QSpacerItem(20,2), 0, 2 );
	glayCoords->addItem( new QSpacerItem(20,2), 1, 2 );
	glayCoords->addItem( new QSpacerItem(20,2), 2, 2 );
	glayCoords->addWidget( AzLabel, 0, 3 );
	glayCoords->addWidget( AltLabel, 1, 3 );
	glayCoords->addWidget( AirMassLabel, 2, 3 );
	glayCoords->addWidget( Az, 0, 4 );
	glayCoords->addWidget( Alt, 1, 4 );
	glayCoords->addWidget( AirMass, 2, 4 );
	vlayMain->addSpacing( 10 );
	vlayMain->addLayout( glayCoords );
}

DetailDialog::RiseSetBox::RiseSetBox( SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, 
		QWidget *parent, const char *name ) : QGroupBox( i18n( "Rise/Set/Transit" ), parent, name ) {

	QTime rt = o->riseSetTime( ut, geo, true ); //true = use rise time
	dms raz = o->riseSetTimeAz( ut, geo, true ); //true = use rise time

	//If transit time is before rise time, use transit time for tomorrow
	QTime tt = o->transitTime( ut, geo );
	dms talt = o->transitAltitude( ut, geo );
	if ( tt < rt ) {
		tt = o->transitTime( ut.addDays( 1 ), geo );
		talt = o->transitAltitude( ut.addDays( 1 ), geo );
	}

	//If set time is before rise time, use set time for tomorrow
	QTime st = o->riseSetTime(  ut, geo, false ); //false = use set time
	dms saz = o->riseSetTimeAz( ut, geo, false ); //false = use set time
	if ( st < rt ) {
		st = o->riseSetTime( ut.addDays( 1 ), geo, false ); //false = use set time
		saz = o->riseSetTimeAz( ut.addDays( 1 ), geo, false ); //false = use set time
	}

	RTimeLabel = new QLabel( i18n( "Rise time:" ), this );
	TTimeLabel = new QLabel( i18n( "Transit time:" ), this );
	STimeLabel = new QLabel( i18n( "the time at which an object falls below the horizon", "Set time:" ), this );
	RAzLabel = new QLabel( i18n( "azimuth of object as it rises above horizon", "Azimuth at rise:" ), this );
	TAltLabel = new QLabel( i18n( "altitude of object as it transits the meridian", "Altitude at transit:" ), this );
	SAzLabel = new QLabel( i18n( "azimuth angle of object as it sets below horizon", "Azimuth at set:" ), this );

	if ( rt.isValid() ) {
		RTime = new QLabel( QString().sprintf( "%02d:%02d", rt.hour(), rt.minute() ), this );
		STime = new QLabel( QString().sprintf( "%02d:%02d", st.hour(), st.minute() ), this );
		RAz = new QLabel( raz.toDMSString(), this );
		SAz = new QLabel( saz.toDMSString(), this );
	} else {
		QString rs, ss;
		if ( o->alt()->Degrees() > 0.0 ) {
			rs = i18n( "Circumpolar" );
			ss = i18n( "Circumpolar" );
		} else {
			rs = i18n( "Never rises" );
			ss = i18n( "Never rises" );
		}

		RTime = new QLabel( rs, this );
		STime = new QLabel( ss, this );
		RAz = new QLabel( i18n( "Not Applicable", "N/A" ), this );
		SAz = new QLabel( i18n( "Not Applicable", "N/A" ), this );
	}

	TTime = new QLabel( QString().sprintf( "%02d:%02d", tt.hour(), tt.minute() ), this );
	TAlt = new QLabel( talt.toDMSString(), this );

	QFont boldFont = RTime->font();
	boldFont.setWeight( QFont::Bold );
	RTime->setFont( boldFont );
	TTime->setFont( boldFont );
	STime->setFont( boldFont );
	RAz->setFont( boldFont );
	TAlt->setFont( boldFont );
	SAz->setFont( boldFont );
	RTime->setAlignment( AlignRight );
	TTime->setAlignment( AlignRight );
	STime->setAlignment( AlignRight );
	RAz->setAlignment( AlignRight );
	TAlt->setAlignment( AlignRight );
	SAz->setAlignment( AlignRight );

//Layout
	vlay = new QVBoxLayout( this, 12 );
	glay = new QGridLayout( 5, 3, 10 ); //ncols, nrows, spacing
	glay->addWidget( RTimeLabel, 0, 0 );
	glay->addWidget( TTimeLabel, 1, 0 );
	glay->addWidget( STimeLabel, 2, 0 );
	glay->addWidget( RTime, 0, 1 );
	glay->addWidget( TTime, 1, 1 );
	glay->addWidget( STime, 2, 1 );
	glay->addItem( new QSpacerItem(20,2), 0, 2 );
	glay->addItem( new QSpacerItem(20,2), 1, 2 );
	glay->addItem( new QSpacerItem(20,2), 2, 2 );
	glay->addWidget( RAzLabel, 0, 3 );
	glay->addWidget( TAltLabel, 1, 3 );
	glay->addWidget( SAzLabel, 2, 3 );
	glay->addWidget( RAz, 0, 4 );
	glay->addWidget( TAlt, 1, 4 );
	glay->addWidget( SAz, 2, 4 );
	vlay->addSpacing( 10 );
	vlay->addLayout( glay );
}

void DetailDialog::unselectInfoList()
{
	infoList->setSelected(infoList->currentItem(), false);
}

void DetailDialog::unselectImagesList()
{
	imagesList->setSelected(imagesList->currentItem(), false);
}

void DetailDialog::viewLink()
{
	QString URL;

	if (infoList->currentItem() != -1 && infoList->isSelected(infoList->currentItem()))
		URL = QString(*selectedObject->InfoList.at(infoList->currentItem()));
	else if (imagesList->currentItem() != -1)
		URL = QString(*selectedObject->ImageList.at(imagesList->currentItem()));

	if (!URL.isEmpty())
		kapp->invokeBrowser(URL);
}

void DetailDialog::updateLists()
{
	infoList->clear();
	imagesList->clear();
	
	QStringList::Iterator itList = selectedObject->InfoList.begin();
	QStringList::Iterator itTitle = selectedObject->InfoTitle.begin();
	QStringList::Iterator itListEnd = selectedObject->InfoList.end();
	
	for ( ; itList != itListEnd; ++itList ) {
		infoList->insertItem(QString(*itTitle));
		itTitle++;
	}

	infoList->setSelected(0, true);
	itList  = selectedObject->ImageList.begin();
	itTitle = selectedObject->ImageTitle.begin();
	itListEnd = selectedObject->ImageList.end();

	for ( ; itList != itListEnd; ++itList ) {
		imagesList->insertItem(QString(*itTitle));
		itTitle++;
	}
}

void DetailDialog::editLinkDialog()
{
	int type;
	uint i;
	QString defaultURL , entry;
	QFile newFile;
	
	KDialogBase editDialog(KDialogBase::Plain, i18n("Edit Link"), Ok|Cancel, Ok , this, "editlink", false);
	QFrame *editFrame = editDialog.plainPage();
	
	editLinkURL = new QLabel(i18n("URL:"), editFrame);
	editLinkField = new QLineEdit(editFrame, "lineedit");
	editLinkField->setMinimumWidth(300);
	editLinkField->home(false);
	editLinkLayout = new QHBoxLayout(editFrame, 6, 6, "editlinklayout");
	editLinkLayout->addWidget(editLinkURL);
	editLinkLayout->addWidget(editLinkField);
	
	currentItemIndex = infoList->currentItem();
	
	if (currentItemIndex != -1 && infoList->isSelected(currentItemIndex))
	{
		defaultURL = *selectedObject->InfoList.at(currentItemIndex);
		editLinkField->setText(defaultURL);
		type = 1;
		currentItemTitle = infoList->currentText();
	}
	else if ( (currentItemIndex = imagesList->currentItem()) != -1)
	{
		defaultURL = *selectedObject->ImageList.at(currentItemIndex);
		editLinkField->setText(defaultURL);
		type = 0;
		currentItemTitle = imagesList->currentText();
	}
	else return;

	// If user presses cancel then return
	if (!editDialog.exec() == QDialog::Accepted)
		return;
	// if it wasn't edit, don't do anything
	if (!editLinkField->edited())
		return;

	// Save the URL of the current item
	currentItemURL =  editLinkField->text();
	entry = selectedObject->name() + ":" + currentItemTitle + ":" + currentItemURL;

	switch (type)
	{
		case 0:
			if (!verifyUserData(type))
				return;
				break;
		case 1:
			if (!verifyUserData(type))
				return;
				break;
	}

	// Open a new file with the same name and copy all data along with changes
	newFile.setName(file.name());
	newFile.open(IO_WriteOnly);

	QTextStream newStream(&newFile);

	for (i=0; i<dataList.count(); i++)
	{
		newStream << dataList[i] << endl;
		continue;
	}

	if (type==0)
	{
		*selectedObject->ImageTitle.at(currentItemIndex) = currentItemTitle;
		*selectedObject->ImageList.at(currentItemIndex) = currentItemURL;
	}
	else
	{
		*selectedObject->InfoTitle.at(currentItemIndex) = currentItemTitle;
		*selectedObject->InfoList.at(currentItemIndex) = currentItemURL;
	}

	newStream << entry << endl;

	newFile.close();
	file.close();
	updateLists();
}

void DetailDialog::removeLinkDialog()
{
	int type;
	uint i;
	QString defaultURL, entry;
	QFile newFile;
	
	currentItemIndex = infoList->currentItem();
	
	if (currentItemIndex != -1 && infoList->isSelected(currentItemIndex))
	{
		defaultURL = *selectedObject->InfoList.at(currentItemIndex);
		type = 1;
		currentItemTitle = infoList->currentText();
	}
	else
	{
		currentItemIndex = imagesList->currentItem();
		defaultURL = *selectedObject->ImageList.at(currentItemIndex);
		type = 0;
		currentItemTitle = imagesList->currentText();
	}

	if (KMessageBox::questionYesNoCancel( 0, i18n("Are you sure you want to remove the %1 link?").arg(currentItemTitle), i18n("Delete Confirmation"))!=KMessageBox::Yes)
		return;

	switch (type)
	{
		case 0:
			if (!verifyUserData(type))
				return;
			selectedObject->ImageTitle.remove( selectedObject->ImageTitle.at(currentItemIndex));
			selectedObject->ImageList.remove( selectedObject->ImageList.at(currentItemIndex));
			break;

		case 1:
			if (!verifyUserData(type))
				return;
			selectedObject->InfoTitle.remove(selectedObject->InfoTitle.at(currentItemIndex));
			selectedObject->InfoList.remove(selectedObject->InfoList.at(currentItemIndex));
			break;
	}

	// Open a new file with the same name and copy all data along with changes
	newFile.setName(file.name());
	newFile.open(IO_WriteOnly);

	QTextStream newStream(&newFile);

	for (i=0; i<dataList.count(); i++)
		newStream << dataList[i] << endl;

	newFile.close();
	file.close();
	updateLists();
}

bool DetailDialog::verifyUserData(int type)
{
	QString line, name, sub, title;
	bool ObjectFound = false;
	uint i;
	
	switch (type)
	{
		case 0:
			if (!readUserFile(type))
				return false;
			for (i=0; i<dataList.count(); i++)
			{
				line = dataList[i];
				name = line.mid( 0, line.find(':') );
				sub = line.mid( line.find(':')+1 );
				title = sub.mid( 0, sub.find(':') );
				if (name == selectedObject->name() && title == currentItemTitle)
				{
					ObjectFound = true;
					dataList.remove(dataList.at(i));
					break;
				}
			}
			break;
		case 1:
			if (!readUserFile(type))
				return false;
			for (i=0; i<dataList.count(); i++)
			{
				line = dataList[i];
				name = line.mid( 0, line.find(':') );
				sub = line.mid( line.find(':')+1 );
				title = sub.mid( 0, sub.find(':') );
				if (name == selectedObject->name() && title == currentItemTitle)
				{
					ObjectFound = true;
					dataList.remove(dataList.at(i));
					break;
				}
			}
			break;
	}
	return ObjectFound;
}

bool DetailDialog::readUserFile(int type)//, int sourceFileType)
{
	switch (type)
	{
		case 0:
			file.setName( locateLocal( "appdata", "image_url.dat" ) ); //determine filename
			if ( !file.open( IO_ReadOnly) )
			{
				ksw->data()->initError("image_url.dat", false);
				return false;
			}
			break;

		case 1:
			file.setName( locateLocal( "appdata", "info_url.dat" ) );  //determine filename
			if ( !file.open( IO_ReadOnly) )
			{
				ksw->data()->initError("info_url.dat", false);
				return false;
			}
			break;
	}

	// Must reset file
	file.reset();
	QTextStream stream(&file);

	dataList.clear();
	
	// read all data into memory
	while (!stream.eof())
		dataList.append(stream.readLine());

	return true;
}

void DetailDialog::populateADVTree(QListViewItem *parent)
{
	// list done
	if (!treeIt->current())
		return;

	// if relative top level [KSLABEL]
	if (treeIt->current()->Type == 0)
		forkTree(parent);

	while (treeIt->current())
	{
		if (treeIt->current()->Type == 0)
		{
			forkTree(parent);
			continue;
		}
		else if (treeIt->current()->Type == 1)
			break;

		if (parent)
			new QListViewItem(parent, treeIt->current()->Name);
		else
			new QListViewItem(ADVTree, treeIt->current()->Name);

		++(*treeIt);
	}
}

void DetailDialog::forkTree(QListViewItem *parent)
{
	QListViewItem *current = 0;
	if (parent)
		current = new QListViewItem(parent, treeIt->current()->Name);
	else
		current = new QListViewItem(ADVTree, treeIt->current()->Name);

	// we need to increment the iterator before and after populating the tree
	++(*treeIt);
	populateADVTree(current);
	++(*treeIt);
}

void  DetailDialog::viewADVData()
{
	QString link;
	QListViewItem * current = ADVTree->currentItem();

	if (!current)  return;

	treeIt->toFirst();
	while (treeIt->current())
	{
		if (treeIt->current()->Name == current->text(0))
		{
			if (treeIt->current()->Type == 2)  break;
			else return;
		}
		++(*treeIt);
	}

	link = treeIt->current()->Link;
	link = parseADVData(link);
	kapp->invokeBrowser(link);
}

QString DetailDialog::parseADVData(QString link)
{
	QString subLink;
	int index;
	
	if ( (index = link.find("KSOBJ")) != -1)
	{
		link.remove(index, 5);
		link = link.insert(index, selectedObject->name());
	}

	if ( (index = link.find("KSRA")) != -1)
	{
		link.remove(index, 4);
		subLink = QString().sprintf("%02d%02d%02d", selectedObject->ra0()->hour(), selectedObject->ra0()->minute(), selectedObject->ra0()->second());
		subLink = subLink.insert(2, "%20");
		subLink = subLink.insert(7, "%20");

		link = link.insert(index, subLink);
	}
	if ( (index = link.find("KSDEC")) != -1)
	{
		link.remove(index, 5);
		if (selectedObject->dec()->degree() < 0)
		{
			subLink = QString().sprintf("%03d%02d%02d", selectedObject->dec0()->degree(), selectedObject->dec0()->arcmin(), selectedObject->dec0()->arcsec());
			subLink = subLink.insert(3, "%20");
			subLink = subLink.insert(8, "%20");
		}
		else
		{
			subLink = QString().sprintf("%02d%02d%02d", selectedObject->dec0()->degree(), selectedObject->dec0()->arcmin(), selectedObject->dec0()->arcsec());
			subLink = subLink.insert(0, "%2B");
			subLink = subLink.insert(5, "%20");
			subLink = subLink.insert(10, "%20");
		}
		link = link.insert(index, subLink);
	}

	return link;
}

void DetailDialog::saveLogData() {
  selectedObject->saveUserLog( userLog->text() );
}

#include "detaildialog.moc"
