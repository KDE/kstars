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

#include <qstring.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qfont.h>
#include <qframe.h>
#include <qtabwidget.h>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <klineedit.h>
#include <klistview.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <ktextedit.h>

#include "detaildialog.h"
#include "detaildialogui.h"
#include "addlinkdialog.h"
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

DetailDialog::DetailDialog(SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo,
		QWidget *parent, const char *name ) :
		KDialogBase( KDialogBase::Plain, i18n( "Object Details" ), Close, Close, parent, name ) ,
		selectedObject(o), ksw((KStars*)parent)
{
	QFrame *page = plainPage();
	setMainWidget( page );
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	dd = new DetailDialogUI( page );
	vlay->addWidget( dd );
	
	//Make bold fonts
	QFont bFont = dd->PrimaryName->font();
	bFont.setWeight( QFont::Bold );
	dd->PrimaryName->setFont( bFont );
	dd->Type->setFont( bFont );
	dd->Mag->setFont( bFont );
	dd->Dist->setFont( bFont );
	dd->AngSize->setFont( bFont );
	dd->RA->setFont( bFont );
	dd->Dec->setFont( bFont );
	dd->HA->setFont( bFont );
	dd->Az->setFont( bFont );
	dd->Alt->setFont( bFont );
	dd->AirMass->setFont( bFont );
	dd->RTime->setFont( bFont );
	dd->TTime->setFont( bFont );
	dd->STime->setFont( bFont );
	dd->RAz->setFont( bFont );
	dd->TAlt->setFont( bFont );
	dd->SAz->setFont( bFont );
	
	initGeneralTab( ut, geo );
	
	//Disable the Links, Advanced, and Log tabs for unnamed stars
	if (selectedObject->name() == QString("star")) {
		dd->tabs->setTabEnabled( dd->tabs->page(1), false );
		dd->tabs->setTabEnabled( dd->tabs->page(2), false );
		dd->tabs->setTabEnabled( dd->tabs->page(3), false );
	} else {
		initLinksTab();
		initAdvancedTab();
		initLogTab();
	}
}

void DetailDialog::initGeneralTab( const KStarsDateTime &ut, GeoLocation *geo )
{
	dms lst = ksw->data()->geo()->GSTtoLST( ut.gst() );

	initNameBox();
	initCoordBox( ut.epoch(), &lst );
	initRiseSetBox( ut, geo );

	connect(dd->CenterButton, SIGNAL(clicked()), this, SLOT(slotCenter()));
}

void DetailDialog::initLinksTab()
{
	QStringList::Iterator itList = selectedObject->InfoList.begin();
	QStringList::Iterator itTitle = selectedObject->InfoTitle.begin();
	QStringList::Iterator itListEnd = selectedObject->InfoList.end();

	for ( ; itList != itListEnd; ++itList ) {
		dd->InfoList->insertItem(QString(*itTitle));
		itTitle++;
	}

	dd->InfoList->setSelected(0, true);

	itList  = selectedObject->ImageList.begin();
	itTitle = selectedObject->ImageTitle.begin();
	itListEnd  = selectedObject->ImageList.end();

	for ( ; itList != itListEnd; ++itList ) {
		dd->ImageList->insertItem(QString(*itTitle));
		itTitle++;
	}

	if (! dd->InfoList->count() && ! dd->ImageList->count())
			dd->EditLinkButton->setDisabled(true);

	//Signals/Slots
	connect(dd->ViewLinkButton, SIGNAL(clicked()), this, SLOT(viewLink()));
	connect(dd->AddLinkButton, SIGNAL(clicked()), ksw->map(), SLOT(addLink()));
	connect(dd->EditLinkButton, SIGNAL(clicked()), this, SLOT(editLinkDialog()));
	connect(dd->RemoveLinkButton, SIGNAL(clicked()), this, SLOT(removeLinkDialog()));
	connect(dd->InfoList, SIGNAL(highlighted(int)), this, SLOT(unselectImagesList()));
	connect(dd->ImageList, SIGNAL(highlighted(int)), this, SLOT(unselectInfoList()));
	connect(ksw->map(), SIGNAL(linkAdded()), this, SLOT(updateLists()));
}

void DetailDialog::initAdvancedTab()
{
	// Disable the Advanced Tab if advinterface file failed to load,
	// or if object is a solar system object
	if ( ksw->data()->ADVtreeList.isEmpty() ||
				selectedObject->type() == SkyObject::PLANET ||
				selectedObject->type() == SkyObject::COMET ||
				selectedObject->type() == SkyObject::ASTEROID ) {
		dd->tabs->setTabEnabled( dd->tabs->page(2), false );
		return;
	}
	
	treeIt = new QPtrListIterator<ADVTreeData> (ksw->data()->ADVtreeList);

	connect(dd->ViewADVLinkButton, SIGNAL(clicked()), this, SLOT(viewADVData()));
	connect(dd->ADVTree, SIGNAL(doubleClicked(QListViewItem*)), this, SLOT(viewADVData()));

	populateADVTree(NULL);
}

void DetailDialog::initLogTab()
{
	if (selectedObject->userLog.isEmpty())
		dd->UserLog->setText(i18n("Record here observation logs and/or data on %1.").arg(selectedObject->translatedName()));
	else
		dd->UserLog->setText(selectedObject->userLog);

	connect(dd->SaveLogButton, SIGNAL(clicked()), this, SLOT(saveLogData()));
}

void DetailDialog::initNameBox() {
	StarObject *s = NULL;
	DeepSkyObject *dso = NULL;
	KSPlanetBase *ps = NULL;

	QString pname( "" ), oname( "" );
	QString typeStr( "" ), distStr("--"), angStr("--"), magStr("--");
	QString typeLabelStr( "" ), sizeLabelStr( "" ), magLabelStr( "" );

	pname = selectedObject->translatedName();
	
	//typeStr is taken from the Object's type, except for the Sun 
	//(the Sun's typeStr is altered below)
	typeStr = selectedObject->typeName();
	
	//magStr is the object's magnitude, except for the Moon
	//(the Moon's magStr is altered below)
	if ( selectedObject->mag() < 90.0 ) 
		magStr = QString("%1").arg( selectedObject->mag() );
	
	//NameBox appearance depends on type of object
	switch ( selectedObject->type() ) {
	
	case 0: //stars
		s = (StarObject *)selectedObject;
		
		//Names
		if ( pname == i18n( "star" ) ) pname = i18n( "Unnamed star" );
		if ( ! s->gname().isEmpty() )
			oname = s->gname();
		
		//Type label string
		typeLabelStr = i18n( "Spectral type:" );
		
		//Distance
		distStr = QString("%1").arg( s->distance(), 0, 'f',1 ) + i18n(" parsecs", " pc");
		if (s->distance() > 2000 || s->distance() < 0)  // parallax < 0.5 mas
			distStr = i18n("larger than 2000 parsecs", "> 2000 pc");

		//Ang. Size label string (blank for stars; we use this field for mult./var.)
		sizeLabelStr = " ";
		
		//multiple/variable
		if ( s->isMultiple() ) angStr += i18n( "the star is a multiple star", "multiple" );
		if ( s->isMultiple() && s->isVariable() ) angStr += ", ";
		if ( s->isVariable() ) angStr += i18n( "the star is a variable star", "variable" );
		
		break;
	
	case 2: //planets
		ps = (KSPlanetBase *)selectedObject;

		//Distance (from Earth).  The moon requires a unit conversion
		if ( ps->name() == "Moon" ) {
			distStr = i18n("distance in kilometers", "%1 km").arg( ps->rearth()*AU_KM, 0, 'f', 1 );
		} else {
			distStr = i18n("distance in Astronomical Units", "%1 AU").arg( ps->rearth(), 0, 'f', 1 );
		}

		//Angular size
		if ( ps->angSize() ) {
			angStr = i18n("angular size in arcseconds", "%1 arcsec").arg( ps->angSize()*60.0, 0, 'f', 1 );
			if ( ps->name() == "Sun" || ps->name() == "Moon" )
				angStr = i18n("angular size in arcminutes", "%1 arcmin").arg( ps->angSize(), 0, 'f', 1 );
		}

		//The Sun's type string must be changed to "star"
		if ( ps->name() == "Sun" ) 
			typeStr = i18n( "star" );
		
		//the Moon displays illumination fraction instead of magnitude
		if ( selectedObject->name() == "Moon" ) {
			magLabelStr = i18n( "Illumination:" );
			magStr = QString("%1 %").arg( int( ((KSMoon *)selectedObject)->illum()*100. ) );
		}
		
		break;
	
	case 9:  //comets
	case 10: //asteroids:
		ps = (KSPlanetBase *)selectedObject;
		distStr = i18n("distance in Astronomical Units", "%1 AU").arg( ps->rearth(), 0, 'f', 1 );
		break;
	
	default: //deep-sky objects
		dso = (DeepSkyObject *)selectedObject;

		//Names
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

		//Add UGC/PGC numbers to "other names"
		if ( dso->ugc() != 0 ) {
			if ( ! oname.isEmpty() ) oname += ", ";
			oname += "UGC " + QString("%1").arg( dso->ugc() );
		}
		if ( dso->pgc() != 0 ) {
			if ( ! oname.isEmpty() ) oname += ", ";
			oname += "PGC " + QString("%1").arg( dso->pgc() );
		}
		
		//Angular size
		if ( dso->a() ) angStr = i18n("angular size in arcminutes", "%1 arcmin").arg( dso->a() );

		break;
	}

	//Now, populate the Name Box widgets
	dd->PrimaryName->setText( pname );
	dd->OtherNames->setText( oname );
	dd->Type->setText( typeStr );
	dd->Mag->setText( magStr );
	dd->Dist->setText( distStr );
	dd->AngSize->setText( angStr );
	
	if ( ! typeLabelStr.isEmpty() )
		dd->TypeLabel->setText( typeLabelStr );
	if ( ! magLabelStr.isEmpty() )
		dd->MagLabel->setText( magLabelStr );
	if ( ! sizeLabelStr.isEmpty() )
		dd->SizeLabel->setText( sizeLabelStr );
}

void DetailDialog::initCoordBox( double epoch, dms *LST ) {
	dd->RALabel->setText( i18n( "RA (%1):" ).arg( epoch, 7, 'f', 2 ) );
	dd->DecLabel->setText( i18n( "Dec (%1):" ).arg( epoch, 7, 'f', 2 ) );
	
	dd->RA->setText( selectedObject->ra()->toHMSString() );
	dd->Dec->setText( selectedObject->dec()->toDMSString() );

	//Hour Angle can be negative, but dms HMS expressions cannot.
	//Here's a kludgy workaround:
	dms ha( LST->Degrees() - selectedObject->ra()->Degrees() );
	QChar sgn('+');
	if ( ha.Hours() > 12.0 ) {
		ha.setH( 24.0 - ha.Hours() );
		sgn = '-';
	}
	dd->HA->setText( QString("%1%2").arg(sgn).arg(ha.toHMSString()) );

	dd->Az->setText( selectedObject->az()->toDMSString() );
	dd->Alt->setText( selectedObject->alt()->toDMSString() );

	//airmass is approximated as the secant of the zenith distance,
	//equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
	if ( selectedObject->alt()->Degrees() > 0.0 )
		dd->AirMass->setText( QString("%1").arg( 1./sin( selectedObject->alt()->radians() ), 4, 'f', 2 ) );
	else
		dd->AirMass->setText( "--" );
}

void DetailDialog::initRiseSetBox( const KStarsDateTime &ut, GeoLocation *geo ) {
	//Rise Time and Azimuth
	QTime rt = selectedObject->riseSetTime( ut, geo, true ); //true = use rise time
	dms raz = selectedObject->riseSetTimeAz( ut, geo, true ); //true = use rise time

	//Transit Time and Altitude
	//(If transit time is before rise time, use transit time for tomorrow)
	QTime tt = selectedObject->transitTime( ut, geo );
	dms talt = selectedObject->transitAltitude( ut, geo );
	if ( tt < rt ) {
		tt = selectedObject->transitTime( ut.addDays( 1 ), geo );
		talt = selectedObject->transitAltitude( ut.addDays( 1 ), geo );
	}

	//Set Time and Azimuth
	//(If set time is before rise time, use set time for tomorrow)
	QTime st = selectedObject->riseSetTime(  ut, geo, false ); //false = use set time
	dms saz = selectedObject->riseSetTimeAz( ut, geo, false ); //false = use set time
	if ( st < rt ) {
		st = selectedObject->riseSetTime( ut.addDays( 1 ), geo, false ); //false = use set time
		saz = selectedObject->riseSetTimeAz( ut.addDays( 1 ), geo, false ); //false = use set time
	}

	if ( rt.isValid() ) {
		dd->RTime->setText( QString().sprintf( "%02d:%02d", rt.hour(), rt.minute() ) );
		dd->STime->setText( QString().sprintf( "%02d:%02d", st.hour(), st.minute() ) );
		dd->RAz->setText( raz.toDMSString() );
		dd->SAz->setText( saz.toDMSString() );
	} else {
		QString rs, ss;
		if ( selectedObject->alt()->Degrees() > 0.0 ) {
			rs = i18n( "Circumpolar" );
			ss = i18n( "Circumpolar" );
		} else {
			rs = i18n( "Never rises" );
			ss = i18n( "Never rises" );
		}

		dd->RTime->setText( rs );
		dd->STime->setText( ss );
		dd->RAz->setText( i18n( "Not Applicable", "N/A" ) );
		dd->SAz->setText( i18n( "Not Applicable", "N/A" ) );
	}

	dd->TTime->setText( QString().sprintf( "%02d:%02d", tt.hour(), tt.minute() ) );
	dd->TAlt->setText( talt.toDMSString() );
}

void DetailDialog::slotCenter() {
	ksw->map()->setClickedObject( selectedObject );
	ksw->map()->setClickedPoint( selectedObject );
	ksw->map()->slotCenter();
}

void DetailDialog::unselectInfoList()
{
	dd->InfoList->setSelected( dd->InfoList->currentItem(), false );
}

void DetailDialog::unselectImagesList()
{
	dd->ImageList->setSelected( dd->ImageList->currentItem(), false );
}

void DetailDialog::viewLink()
{
	QString URL;

	if ( dd->InfoList->currentItem() != -1 && 
				dd->InfoList->isSelected( dd->InfoList->currentItem() ) )
		URL = QString( *selectedObject->InfoList.at( dd->InfoList->currentItem() ) );
	else if ( dd->ImageList->currentItem() != -1)
		URL = QString( *selectedObject->ImageList.at( dd->ImageList->currentItem() ) );

	if (!URL.isEmpty())
		kapp->invokeBrowser( URL );
}

void DetailDialog::updateLists()
{
	dd->InfoList->clear();
	dd->ImageList->clear();

	QStringList::Iterator itList = selectedObject->InfoList.begin();
	QStringList::Iterator itTitle = selectedObject->InfoTitle.begin();
	QStringList::Iterator itListEnd = selectedObject->InfoList.end();

	for ( ; itList != itListEnd; ++itList ) {
		dd->InfoList->insertItem( QString(*itTitle) );
		itTitle++;
	}

	dd->InfoList->setSelected( 0, true );
	
	itList  = selectedObject->ImageList.begin();
	itTitle = selectedObject->ImageTitle.begin();
	itListEnd = selectedObject->ImageList.end();

	for ( ; itList != itListEnd; ++itList ) {
		dd->ImageList->insertItem( QString(*itTitle) );
		itTitle++;
	}
}

void DetailDialog::editLinkDialog()
{
	int originalType, currentInfoItem, currentImageItem;
	uint i;
	QString originalURL, originalDesc, entry;
	QFile newFile;

	AddLinkDialog ald( this );
	
	currentInfoItem = dd->InfoList->currentItem();
	currentImageItem = dd->ImageList->currentItem();

	if ( currentInfoItem != -1 && dd->InfoList->isSelected( currentInfoItem ) )
	{
		//Info Link selected
		originalURL  = *selectedObject->InfoList.at( currentInfoItem );
		originalDesc = *selectedObject->InfoTitle.at( currentInfoItem );
		ald.setURL( originalURL );
		ald.setDesc( originalDesc );
		originalType = 0;
	} else if ( currentImageItem != -1 && dd->ImageList->isSelected( currentImageItem ) ) {
		//Image Link selected
		originalURL  = *selectedObject->ImageList.at( currentImageItem );
		originalDesc = *selectedObject->ImageTitle.at( currentImageItem );
		ald.setURL( originalURL );
		ald.setDesc( originalDesc );
		originalType = 1;
	} else 
		//uh oh!  no link selected?
		return;

	// If user pressed cancel, stop here
	if ( ! ald.exec() == QDialog::Accepted )
		return;
	
	// if nothing was changed, stop here
	bool changed(false);
	if ( ald.url() != originalURL ) changed = true;
	if ( ald.desc() != originalDesc ) changed = true;
	if ( ald.isImageLink() != originalType ) changed = true;
	if ( ! changed )
		return;

	//construct the modified line for the URL data file
	entry = selectedObject->name() + ":" + ald.desc() + ":" + ald.url();

	//Remove the currently-selected object's existing entry in the URL file
	//(this opens the existing URL file, and populates dataList with its 
	//contents, except for the current object)
	if ( ! removeExistingEntry( originalType, originalDesc ) )
		return;

	//Open a new URL file with the same name and copy dataList into it
	newFile.setName(file.name());
	newFile.open(IO_WriteOnly);

	QTextStream newStream(&newFile);
	for (i=0; i<dataList.count(); i++)
	{
		newStream << dataList[i] << endl;
		continue;
	}

	//Add the new information to the selected object's ImageList/InfoList
	if ( ald.isImageLink() )
	{
		*selectedObject->ImageTitle.at(currentImageItem) = ald.desc();
		*selectedObject->ImageList.at(currentImageItem) = ald.url();
	} else {
		*selectedObject->InfoTitle.at(currentInfoItem) = ald.desc();
		*selectedObject->InfoList.at(currentInfoItem) = ald.url();
	}

	//add the new information for the selected object to the URL file
	newStream << entry << endl;

	newFile.close();
	file.close();
	updateLists();
}

void DetailDialog::removeLinkDialog()
{
	int type(0), currentInfoItem, currentImageItem;
	uint i;
	QString linkName, entry;
	QFile newFile;

	currentInfoItem = dd->InfoList->currentItem();
	currentImageItem = dd->ImageList->currentItem();

	if (currentInfoItem != -1 && dd->InfoList->isSelected(currentInfoItem) )
	{
		type = 0;
		linkName = dd->InfoList->currentText();
	} else if ( currentImageItem != -1 && dd->ImageList->isSelected(currentImageItem) ) {
		type = 1;
		linkName = dd->ImageList->currentText();
	}

	if (KMessageBox::questionYesNoCancel( 0, i18n("Are you sure you want to remove the %1 link?").arg(linkName), i18n("Delete Confirmation")) != KMessageBox::Yes)
		return;

	switch (type)
	{
		case 0:
			if ( !removeExistingEntry( type, linkName ) )
				return;
			selectedObject->InfoTitle.remove(selectedObject->InfoTitle.at(currentInfoItem));
			selectedObject->InfoList.remove(selectedObject->InfoList.at(currentInfoItem));
			break;
		case 1:
			if ( !removeExistingEntry( type, linkName ) )
				return;
			selectedObject->ImageTitle.remove( selectedObject->ImageTitle.at(currentImageItem));
			selectedObject->ImageList.remove( selectedObject->ImageList.at(currentImageItem));
			break;
	}

	// Open a new file with the same name and insert the dataList without the removed entry.
	newFile.setName(file.name());
	newFile.open(IO_WriteOnly);

	QTextStream newStream(&newFile);
	for (i=0; i<dataList.count(); i++)
		newStream << dataList[i] << endl;

	newFile.close();
	file.close();
	updateLists();
}

bool DetailDialog::removeExistingEntry( int type, const QString &originalDesc )
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
				if (name == selectedObject->name() && title == originalDesc )
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
				if (name == selectedObject->name() && title == originalDesc )
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
			file.setName( locateLocal( "appdata", "info_url.dat" ) ); //determine filename
			if ( !file.open( IO_ReadOnly) )
			{
				ksw->data()->initError("info_url.dat", false);
				return false;
			}
			break;

		case 1:
			file.setName( locateLocal( "appdata", "image_url.dat" ) );  //determine filename
			if ( !file.open( IO_ReadOnly) )
			{
				ksw->data()->initError("image_url.dat", false);
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
			new QListViewItem(dd->ADVTree, treeIt->current()->Name);

		++(*treeIt);
	}
}

void DetailDialog::forkTree(QListViewItem *parent)
{
	QListViewItem *current = 0;
	if (parent)
		current = new QListViewItem(parent, treeIt->current()->Name);
	else
		current = new QListViewItem(dd->ADVTree, treeIt->current()->Name);

	// we need to increment the iterator before and after populating the tree
	++(*treeIt);
	populateADVTree(current);
	++(*treeIt);
}

void  DetailDialog::viewADVData()
{
	QString link;
	QListViewItem * current = dd->ADVTree->currentItem();

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

void DetailDialog::saveLogData()
{
	QFile file;
	QString logs;
	QString currentLog = dd->UserLog->text();

	if (currentLog == (i18n("Record here observation logs and/or data on ") + selectedObject->name()))
		return;

	// A label to identiy a header
	QString KSLabel ="[KSLABEL:" + selectedObject->name() + "]";

	file.setName( locateLocal( "appdata", "userlog.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly))
	{
		QTextStream instream(&file);
		// read all data into memory
		logs = instream.read();
		file.close();
	}

	// delete old data
	if (!selectedObject->userLog.isEmpty())
	{
		int startIndex, endIndex;
		QString sub;

		startIndex = logs.find(KSLabel);
		sub = logs.mid (startIndex);
		endIndex = sub.find("[KSLogEnd]");

		logs.remove(startIndex, endIndex + 11);
	}

	selectedObject->userLog = currentLog;

	// append log to existing logs
	if (!currentLog.isEmpty())
		logs.append( KSLabel + "\n" + currentLog + "\n[KSLogEnd]\n");

	if ( !file.open( IO_WriteOnly))
	{
		QString message = i18n( "user log file could not be opened.\nCurrent user log cannot be recorded for future sessions." );
		KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
		return;
	}

	QTextStream outstream(&file);
	outstream << logs;

	KMessageBox::information(0, i18n("The log was saved successfully."));
	file.close();
}

#include "detaildialog.moc"
