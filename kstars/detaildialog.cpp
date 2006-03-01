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
#include <qlayout.h> //still needed for secondary dialogs
#include <qlineedit.h>
#include <qimage.h>
#include <qregexp.h>
//Added by qt3to4:
#include <QPixmap>
#include <QFocusEvent>
#include <QTextStream>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QTreeWidgetItem>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kactivelabel.h>
#include <kpushbutton.h>
#include <klistview.h>
#include <klineedit.h>
#include <ktoolinvocation.h>
#include <kdialogbase.h>

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
#include "thumbnailpicker.h"

#include "indielement.h"
#include "indiproperty.h"
#include "indidevice.h"
#include "indimenu.h"
#include "devicemanager.h"
#include "indistd.h"

DetailDialog::DetailDialog(SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, 
		QWidget *parent ) : 
		QDialog(parent)
		/*KDialogBase( KDialogBase::Tabbed, i18n( "Object Details" ), Close, Close, parent )*/ ,
		selectedObject(o), ksw((KStars*)parent), Data(0), Pos(0), Links(0), Adv(0), Log(0)
{
	//Modify color palette
	setPaletteBackgroundColor( palette().color( QPalette::Active, QColorGroup::Base ) );
	setPaletteForegroundColor( palette().color( QPalette::Active, QColorGroup::Text ) );

	// Create tab widget
	QVBoxLayout *vlay = new QVBoxLayout(this);
	tabWidget = new QTabWidget(this);
	vlay->addWidget(tabWidget);

	QHBoxLayout *hlay = new QHBoxLayout();
	closeButton = new QPushButton(i18n("&Close"), this);
	hlay->addStretch();
	hlay->addWidget(closeButton);

	vlay->addLayout(hlay);

       setWindowTitle(i18n( "Object Details" ));
	
	//Create thumbnail image
	Thumbnail = new QPixmap( 200, 200 );

	createGeneralTab();
	createPositionTab( ut, geo );
	createLinksTab();
	createAdvancedTab();
	createLogTab();

	//Connections
	connect( Data->ObsListButton, SIGNAL( clicked() ), this, SLOT( addToObservingList() ) );
	connect( Data->CenterButton, SIGNAL( clicked() ), this, SLOT( centerMap() ) );
	connect( Data->ScopeButton, SIGNAL( clicked() ), this, SLOT( centerTelescope() ) );
	connect( Data->Image, SIGNAL( clicked() ), this, SLOT( updateThumbnail() ) );
	connect (closeButton, SIGNAL(clicked()), this, SLOT(reject()));
}

void DetailDialog::createGeneralTab()
{
	QWidget *DataTab = new QWidget();
	Data = new DataWidget(DataTab);

	//Show object thumbnail image
	showThumbnail();

	//Fill in the data fields
	//Contents depend on type of object
	StarObject *s = 0L;
	DeepSkyObject *dso = 0L;
	KSPlanetBase *ps = 0L;
	QString pname, oname;

	switch ( selectedObject->type() ) {
	case 0: //stars
		s = (StarObject *)selectedObject;

		Data->Names->setText( s->longname() );
		Data->Type->setText( s->sptype() + " " + i18n("star") );
		Data->Magnitude->setText( i18n( "number in magnitudes", "%1 mag" ).arg(
				KGlobal::locale()->formatNumber( s->mag(), 1 ) ) );  //show to tenths place

		//distance
		if ( s->distance() > 2000. || s->distance() < 0. )  // parallax < 0.5 mas 
			Data->Distance->setText( QString(i18n("larger than 2000 parsecs", "> 2000 pc") ) );
		else if ( s->distance() > 50.0 ) //show to nearest integer
			Data->Distance->setText( i18n( "number in parsecs", "%1 pc" ).arg(
					QString::number( int( s->distance() + 0.5 ) ) ) );
		else if ( s->distance() > 10.0 ) //show to tenths place
			Data->Distance->setText( i18n( "number in parsecs", "%1 pc" ).arg(
					KGlobal::locale()->formatNumber( s->distance(), 1 ) ) );
		else //show to hundredths place
			Data->Distance->setText( i18n( "number in parsecs", "%1 pc" ).arg(
					KGlobal::locale()->formatNumber( s->distance(), 2 ) ) );

		//Note multiplicity/variablility in angular size label
		Data->AngSizeLabel->setText( QString() );
		Data->AngSize->setText( QString() );
		Data->AngSizeLabel->setFont( Data->AngSize->font() );
		if ( s->isMultiple() && s->isVariable() ) {
			Data->AngSizeLabel->setText( i18n( "the star is a multiple star", "multiple" ) + "," );
			Data->AngSize->setText( i18n( "the star is a variable star", "variable" ) );
		} else if ( s->isMultiple() ) 
			Data->AngSizeLabel->setText( i18n( "the star is a multiple star", "multiple" ) );
		else if ( s->isVariable() ) 
			Data->AngSizeLabel->setText( i18n( "the star is a variable star", "variable" ) );
		
		break; //end of stars case

	case 9:  //asteroids [fall through to planets]
	case 10: //comets [fall through to planets]
	case 2:  //planets (including comets and asteroids)
		ps = (KSPlanetBase *)selectedObject;
		
		Data->Names->setText( ps->longname() );
		//Type is "G5 star" for Sun
		if ( ps->name() == "Sun" )
			Data->Type->setText( i18n("G5 star") );
		else
			Data->Type->setText( ps->typeName() );

		//Magnitude: The moon displays illumination fraction instead
		if ( selectedObject->name() == "Moon" ) {
			Data->MagLabel->setText( i18n("Illumination:") );
			Data->Magnitude->setText( QString("%1 %").arg( int( ((KSMoon *)selectedObject)->illum()*100. ) ) );
		} else {
			Data->Magnitude->setText( i18n( "number in magnitudes", "%1 mag" ).arg(
					KGlobal::locale()->formatNumber( ps->mag(), 1 ) ) );  //show to tenths place
		}

		//Distance from Earth.  The moon requires a unit conversion
		if ( ps->name() == "Moon" ) {
			Data->Distance->setText( i18n("distance in kilometers", "%1 km").arg( 
						KGlobal::locale()->formatNumber( ps->rearth()*AU_KM ) ) );
		} else {
			Data->Distance->setText( i18n("distance in Astronomical Units", "%1 AU").arg( 
						KGlobal::locale()->formatNumber( ps->rearth() ) ) );
		}

		//Angular size; moon and sun in arcmin, others in arcsec
		if ( ps->angSize() ) {
			if ( ps->name() == "Sun" || ps->name() == "Moon" ) 
				Data->AngSize->setText( i18n("angular size in arcminutes", "%1 arcmin").arg( 
							KGlobal::locale()->formatNumber( ps->angSize() ) ) );
			else
				Data->AngSize->setText( i18n("angular size in arcseconds", "%1 arcsec").arg( 
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
			oname += "UGC " + QString("%1").arg( dso->ugc() );
		}
		if ( dso->pgc() != 0 ) {
			if ( ! oname.isEmpty() ) oname += ", ";
			oname += "PGC " + QString("%1").arg( dso->pgc() );
		}
		
		if ( ! oname.isEmpty() ) pname += ", " + oname;
		Data->Names->setText( pname );

		Data->Type->setText( dso->typeName() );

		if ( dso->mag() > 90.0 )
			Data->Magnitude->setText( "--" );
		else
			Data->Magnitude->setText( i18n( "number in magnitudes", "%1 mag" ).arg(
					KGlobal::locale()->formatNumber( dso->mag(), 1 ) ) );  //show to tenths place

		//No distances at this point...
		Data->Distance->setText( "--" );

		//Only show decimal place for small angular sizes
		if ( dso->a() > 10.0 ) 
			Data->AngSize->setText( i18n("angular size in arcminutes", "%1 arcmin").arg( 
					int( dso->a() ) ) );
		else if ( dso->a() ) 
			Data->AngSize->setText( i18n("angular size in arcminutes", "%1 arcmin").arg( 
					KGlobal::locale()->formatNumber( dso->a(), 1 ) ) );
		else 
			Data->AngSize->setText( "--" );
		
		break;
	}

	//Common to all types:
	Data->Constellation->setText( ksw->data()->skyComposite()->constellation( selectedObject ) );

	tabWidget->addTab(DataTab, i18n("General"));
}

void DetailDialog::createPositionTab( const KStarsDateTime &ut, GeoLocation *geo ) {
	QWidget *PosTab = new QWidget();
	Pos = new PositionWidget(PosTab);

	//Coordinates Section:
	//Don't use KLocale::formatNumber() for the epoch string,
	//because we don't want a thousands-place separator!
	QString sEpoch = QString::number( ut.epoch(), 'f', 1 );
	//Replace the decimal point with localized decimal symbol
	sEpoch.replace( ".", KGlobal::locale()->decimalSymbol() );

	Pos->RALabel->setText( i18n( "RA (%1):" ).arg( sEpoch ) );
	Pos->DecLabel->setText( i18n( "Dec (%1):" ).arg( sEpoch ) );
	Pos->RA->setText( selectedObject->ra()->toHMSString() );
	Pos->Dec->setText( selectedObject->dec()->toDMSString() );
	Pos->Az->setText( selectedObject->az()->toDMSString() );
	Pos->Alt->setText( selectedObject->alt()->toDMSString() );

	//Hour Angle can be negative, but dms HMS expressions cannot.
	//Here's a kludgy workaround:
	dms lst = geo->GSTtoLST( ut.gst() );
	dms ha( lst.Degrees() - selectedObject->ra()->Degrees() );
	QChar sgn('+');
	if ( ha.Hours() > 12.0 ) {
		ha.setH( 24.0 - ha.Hours() );
		sgn = '-';
	}
	Pos->HA->setText( QString("%1%2").arg(sgn).arg( ha.toHMSString() ) );

	//Airmass is approximated as the secant of the zenith distance,
	//equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
	if ( selectedObject->alt()->Degrees() > 0.0 ) 
		Pos->Airmass->setText( KGlobal::locale()->formatNumber( 
				1./sin( selectedObject->alt()->radians() ), 2 ) );
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
		if ( selectedObject->alt()->Degrees() > 0.0 ) {
			Pos->TimeRise->setText( i18n( "Circumpolar" ) );
			Pos->TimeSet->setText( i18n( "Circumpolar" ) );
		} else {
			Pos->TimeRise->setText( i18n( "Never rises" ) );
			Pos->TimeSet->setText( i18n( "Never rises" ) );
		}

		Pos->AzRise->setText( i18n( "Not Applicable", "N/A" ) );
		Pos->AzSet->setText( i18n( "Not Applicable", "N/A" ) );
	}

	Pos->TimeTransit->setText( QString().sprintf( "%02d:%02d", tt.hour(), tt.minute() ) );
	Pos->AltTransit->setText( talt.toDMSString() );

	tabWidget->addTab(PosTab, i18n("Position"));
}

void DetailDialog::createLinksTab()
{
	// don't create a link tab for an unnamed star
	if (selectedObject->name() == QString("star"))
		return;

	QWidget *LinksTab = new QWidget();
	Links = new LinksWidget(LinksTab);

	foreach ( QString s, selectedObject->InfoList )
		Links->InfoList->insertItem( s );

	Links->InfoList->setSelected(0, true);

	foreach ( QString s, selectedObject->ImageList )
		Links->ImagesList->insertItem( s );

	if ( ! Links->InfoList->count() && ! Links->ImagesList->count() ) {
		Links->EditLinkButton->setDisabled(true);
		Links->RemoveLinkButton->setDisabled(true);
	}

	// Signals/Slots
	connect( Links->ViewButton, SIGNAL(clicked()), this, SLOT( viewLink() ) );
	connect( Links->AddLinkButton, SIGNAL(clicked()), ksw->map(), SLOT( addLink() ) );
	connect( Links->EditLinkButton, SIGNAL(clicked()), this, SLOT( editLinkDialog() ) );
	connect( Links->RemoveLinkButton, SIGNAL(clicked()), this, SLOT( removeLinkDialog() ) );
	connect( Links->InfoList, SIGNAL(highlighted(int)), this, SLOT( unselectImagesList() ) );
	connect( Links->ImagesList, SIGNAL(highlighted(int)), this, SLOT( unselectInfoList() ) );
	connect( ksw->map(), SIGNAL(linkAdded()), this, SLOT( updateLists() ) );

	tabWidget->addTab(LinksTab, i18n( "Links" ));
}

void DetailDialog::createAdvancedTab()
{
	// Don't create an adv tab for an unnamed star or if advinterface file failed loading
	// We also don't need adv dialog for solar system objects.
	if (selectedObject->name() == QString("star") || 
				ksw->data()->ADVtreeList.isEmpty() || 
				selectedObject->type() == SkyObject::PLANET || 
				selectedObject->type() == SkyObject::COMET || 
				selectedObject->type() == SkyObject::ASTEROID )
		return;

	QWidget *AdvancedTab = new QWidget();
	Adv = new DatabaseWidget(AdvancedTab );

	//treeIt = new Q3PtrListIterator<ADVTreeData> (ksw->data()->ADVtreeList);
	connect( Adv->ADVTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(viewADVData()));

	populateADVTree();

	tabWidget->addTab(AdvancedTab, i18n("Advanced"));
}

void DetailDialog::createLogTab()
{
	//Don't create a a log tab for an unnamed star
	if ( selectedObject->name() == QString("star") )
		return;

	// Log Tab
	QWidget *LogTab = new QWidget();
	Log = new LogWidget(LogTab);

	if ( selectedObject->userLog.isEmpty() )
		Log->UserLog->setText(i18n("Record here observation logs and/or data on %1.").arg(selectedObject->translatedName()));
	else
		Log->UserLog->setText(selectedObject->userLog);

	//Automatically save the log contents when the widget loses focus
	connect( Log->UserLog, SIGNAL( focusOut() ), this, SLOT( saveLogData() ) );

	tabWidget->addTab(LogTab,i18n("Log")); 
}


void DetailDialog::unselectInfoList()
{
	Links->InfoList->setSelected( Links->InfoList->currentItem(), false );
}

void DetailDialog::unselectImagesList()
{
	Links->ImagesList->setSelected( Links->ImagesList->currentItem(), false );
}

void DetailDialog::viewLink()
{
	QString URL;

	if ( Links->InfoList->currentItem() != -1 && 
			Links->InfoList->isSelected( Links->InfoList->currentItem() ) )
		URL = QString( selectedObject->InfoList.at( Links->InfoList->currentItem() ) );
	else if ( Links->ImagesList->currentItem() != -1 )
		URL = QString( selectedObject->ImageList.at( Links->ImagesList->currentItem() ) );

	if ( !URL.isEmpty() )
		KToolInvocation::invokeBrowser(URL);
}

void DetailDialog::updateLists()
{
	Links->InfoList->clear();
	Links->ImagesList->clear();
	
	foreach ( QString s, selectedObject->InfoList ) 
		Links->InfoList->insertItem( s );

	Links->InfoList->setSelected(0, true);

	foreach ( QString s, selectedObject->ImageList ) 
		Links->ImagesList->insertItem( s );
}

void DetailDialog::editLinkDialog()
{
	int type;
	int i;
	QString defaultURL , entry;
	QFile newFile;
	
	/* FIXME use QDialog or KDialog

KDialogBase editDialog(KDialogBase::Plain, i18n("Edit Link"), Ok|Cancel, Ok , this, "editlink", false);
	QFrame *editFrame = editDialog.plainPage();
	
	editLinkURL = new QLabel(i18n("URL:"), editFrame);
	editLinkField = new QLineEdit(editFrame, "lineedit");
	editLinkField->setMinimumWidth(300);
	editLinkField->home(false);
	editLinkLayout = new QHBoxLayout(editFrame, 6, 6, "editlinklayout");
	editLinkLayout->addWidget(editLinkURL);
	editLinkLayout->addWidget(editLinkField);
	
	currentItemIndex = Links->InfoList->currentItem();
	
	if (currentItemIndex != -1 && Links->InfoList->isSelected( currentItemIndex ) )
	{
		defaultURL = selectedObject->InfoList.at( currentItemIndex );
		editLinkField->setText( defaultURL );
		type = 1;
		currentItemTitle = Links->InfoList->currentText();
	}
	else if ( (currentItemIndex = Links->ImagesList->currentItem()) != -1)
	{
		defaultURL = selectedObject->ImageList.at( currentItemIndex );
		editLinkField->setText( defaultURL );
		type = 0;
		currentItemTitle = Links->ImagesList->currentText();
	}
	else return;

	// If user presses cancel then return
	if ( ! editDialog.exec() == QDialog::Accepted )
		return;

	// if it wasn't edit, don't do anything
	if ( ! editLinkField->edited() )
		return;

	// Save the URL of the current item
	currentItemURL =  editLinkField->text();
	entry = selectedObject->name() + ":" + currentItemTitle + ":" + currentItemURL;

	//FIXME: usage of verifyUserData() is pretty unclear
	//verifyUserData() returns false if currentItemTitle/currentItemURL 
	//are not found in the user's list already.  If they are, then that 
	//item is removed.
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
	newFile.open(QIODevice::WriteOnly);

	QTextStream newStream(&newFile);

	for (i=0; i<dataList.size(); i++)
	{
		newStream << dataList[i] << endl;
		continue;
	}

	if (type==0)
	{
		selectedObject->ImageTitle.replace(currentItemIndex, currentItemTitle);
		selectedObject->ImageList.replace(currentItemIndex, currentItemURL);
	}
	else
	{
		selectedObject->InfoTitle.replace(currentItemIndex, currentItemTitle);
		selectedObject->InfoList.replace(currentItemIndex, currentItemURL);
	}

	newStream << entry << endl;

	newFile.close();
	file.close();
	updateLists();  */
}

void DetailDialog::removeLinkDialog()
{
	int type;
	int i;
	QString defaultURL, entry;
	QFile newFile;
	
	currentItemIndex = Links->InfoList->currentItem();
	
	if (currentItemIndex != -1 && Links->InfoList->isSelected(currentItemIndex))
	{
		defaultURL = selectedObject->InfoList.at(currentItemIndex);
		type = 1;
		currentItemTitle = Links->InfoList->currentText();
	}
	else
	{
		currentItemIndex = Links->ImagesList->currentItem();
		defaultURL = selectedObject->ImageList.at(currentItemIndex);
		type = 0;
		currentItemTitle = Links->ImagesList->currentText();
	}

	if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove the %1 link?").arg(currentItemTitle), i18n("Delete Confirmation"),KStdGuiItem::del())!=KMessageBox::Continue)
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
	newFile.open(QIODevice::WriteOnly);

	QTextStream newStream(&newFile);

	for (i=0; i<dataList.size(); i++)
		newStream << dataList[i] << endl;

	newFile.close();
	file.close();
	updateLists();
}

bool DetailDialog::verifyUserData(int type)
{
	QString line, name, sub, title;
	bool ObjectFound = false;
	int i;
	
	switch (type)
	{
		case 0:
			if (!readUserFile(type))
				return false;
			for (i=0; i<dataList.size(); i++)
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
			for (i=0; i<dataList.size(); i++)
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
			if ( !file.open( QIODevice::ReadOnly) )
			{
				ksw->data()->initError("image_url.dat", false);
				return false;
			}
			break;

		case 1:
			file.setName( locateLocal( "appdata", "info_url.dat" ) );  //determine filename
			if ( !file.open( QIODevice::ReadOnly) )
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
	while ( ! stream.atEnd() )
		dataList.append(stream.readLine());

	return true;
}

void DetailDialog::populateADVTree()
{
	QTreeWidgetItem *parent = NULL, *temp = NULL;


        foreach (ADVTreeData *item, ksw->data()->ADVtreeList)
	{

		switch (item->Type)
		{
			// Top Level
			case 0:
			//kDebug() << "Level 0 START" << endl;
			temp = new QTreeWidgetItem(parent, QStringList(item->Name));
			if (parent == NULL)
				Adv->ADVTree->addTopLevelItem(temp);
			parent = temp;

			break;

			// End of top level
			case 1:
			//kDebug() << "Level 0 END" << endl;
			if (parent != NULL) parent = parent->parent();
			break;

			// Leaf
			case 2:
 			//kDebug() << "Leaf" << endl;
			new QTreeWidgetItem(parent, QStringList(item->Name));
			break;
		}
	}
	
}

/*void DetailDialog::forkTree(Q3ListViewItem *parent)
{
	Q3ListViewItem *current = 0;
	if (parent)
		current = new Q3ListViewItem(parent, treeIt->current()->Name);
	else
		current = new Q3ListViewItem(Adv->ADVTree, treeIt->current()->Name);

	// we need to increment the iterator before and after populating the tree
	++(*treeIt);
	populateADVTree(current);
	++(*treeIt);
}*/

void  DetailDialog::viewADVData()
{
	QString link;
	QTreeWidgetItem * current = Adv->ADVTree->currentItem();

	if (!current)  return;

	foreach (ADVTreeData *item, ksw->data()->ADVtreeList)
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
	
	if ( (index = link.find("KSOBJ")) != -1)
	{
		link.remove(index, 5);
		link = link.insert(index, selectedObject->name());
	}

	if ( (index = link.find("KSRA")) != -1)
	{
		link.remove(index, 4);
		subLink.sprintf("%02d%02d%02d", selectedObject->ra0()->hour(), selectedObject->ra0()->minute(), selectedObject->ra0()->second());
		subLink = subLink.insert(2, "%20");
		subLink = subLink.insert(7, "%20");

		link = link.insert(index, subLink);
	}
	if ( (index = link.find("KSDEC")) != -1)
	{
		link.remove(index, 5);
		if (selectedObject->dec()->degree() < 0)
		{
			subLink.sprintf("%03d%02d%02d", selectedObject->dec0()->degree(), selectedObject->dec0()->arcmin(), selectedObject->dec0()->arcsec());
			subLink = subLink.insert(3, "%20");
			subLink = subLink.insert(8, "%20");
		}
		else
		{
			subLink.sprintf("%02d%02d%02d", selectedObject->dec0()->degree(), selectedObject->dec0()->arcmin(), selectedObject->dec0()->arcsec());
			subLink = subLink.insert(0, "%2B");
			subLink = subLink.insert(5, "%20");
			subLink = subLink.insert(10, "%20");
		}
		link = link.insert(index, subLink);
	}

	return link;
}

void DetailDialog::saveLogData() {
  selectedObject->saveUserLog( Log->UserLog->text() );
}

void DetailDialog::addToObservingList() {
	ksw->observingList()->slotAddObject( selectedObject );
}

void DetailDialog::centerMap() {
	ksw->map()->setClickedObject( selectedObject );
	ksw->map()->slotCenter();
}

void DetailDialog::centerTelescope()
{

  INDI_D *indidev(NULL);
  INDI_P *prop(NULL), *onset(NULL);
  INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *ConnectEle(NULL), *nameEle(NULL);
  bool useJ2000( false);
  int selectedCoord(0);
  SkyPoint sp;
  
  // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
  // i.e. the first telescope we find!
  
  INDIMenu *imenu = ksw->getINDIMenu();

  
  for ( int i=0; i < imenu->mgr.size() ; i++ )
  {
    for ( int j=0; j < imenu->mgr.at(i)->indi_dev.size(); j++ )
    {
       indidev = imenu->mgr.at(i)->indi_dev.at(j);
       indidev->stdDev->currentObject = NULL;
       prop = indidev->findProp("EQUATORIAL_EOD_COORD");
       	if (prop == NULL)
	{
		  prop = indidev->findProp("EQUATORIAL_COORD");
		  if (prop == NULL)
                  {
                    prop = indidev->findProp("HORIZONTAL_COORD");
                    if (prop == NULL)
        		continue;
                    else
                        selectedCoord = 1;		/* Select horizontal */
                  }
		  else
		        useJ2000 = true;
	}

       ConnectEle = indidev->findElem("CONNECT");
       if (!ConnectEle) continue;
       
       if (ConnectEle->state == PS_OFF)
       {
	 KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.").arg(indidev->label));
	 return;
       }

        switch (selectedCoord)
        {
          // Equatorial
          case 0:
           if (prop->perm == PP_RO) continue;
           RAEle  = prop->findElement("RA");
       	   if (!RAEle) continue;
   	   DecEle = prop->findElement("DEC");
   	   if (!DecEle) continue;
           break;

         // Horizontal
         case 1:
          if (prop->perm == PP_RO) continue;
          AzEle = prop->findElement("AZ");
          if (!AzEle) continue;
          AltEle = prop->findElement("ALT");
          if (!AltEle) continue;
          break;
        }
   
        onset = indidev->findProp("ON_COORD_SET");
        if (!onset) continue;
       
        onset->activateSwitch("SLEW");

        indidev->stdDev->currentObject = selectedObject;

      /* Send object name if available */
      if (indidev->stdDev->currentObject)
         {
             nameEle = indidev->findElem("OBJECT_NAME");
             if (nameEle && nameEle->pp->perm != PP_RO)
             {
                 nameEle->write_w->setText(indidev->stdDev->currentObject->name());
                 nameEle->pp->newText();
             }
          }

       switch (selectedCoord)
       {
         case 0:
            if (indidev->stdDev->currentObject)
		sp.set (indidev->stdDev->currentObject->ra(), indidev->stdDev->currentObject->dec());
            else
                sp.set (ksw->map()->clickedPoint()->ra(), ksw->map()->clickedPoint()->dec());

      	 if (useJ2000)
	    sp.apparentCoord(ksw->data()->ut().djd(), (long double) J2000);

    	   RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
	   DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));

          break;

       case 1:
         if (indidev->stdDev->currentObject)
         {
           sp.setAz(*indidev->stdDev->currentObject->az());
           sp.setAlt(*indidev->stdDev->currentObject->alt());
         }
         else
         {
           sp.setAz(*ksw->map()->clickedPoint()->az());
           sp.setAlt(*ksw->map()->clickedPoint()->alt());
         }

          AzEle->write_w->setText(QString("%1:%2:%3").arg(sp.az()->degree()).arg(sp.az()->arcmin()).arg(sp.az()->arcsec()));
          AltEle->write_w->setText(QString("%1:%2:%3").arg(sp.alt()->degree()).arg(sp.alt()->arcmin()).arg(sp.alt()->arcsec()));

         break;
       }

       prop->newText();
       
       return;
    }
  }
       
  // We didn't find any telescopes
  KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
	
}

void DetailDialog::showThumbnail() {
	//No image if object is a star
	if ( selectedObject->type() == SkyObject::STAR || 
			selectedObject->type() == SkyObject::CATALOG_STAR ) {
		Thumbnail->resize( Data->Image->width(), Data->Image->height() );
		Thumbnail->fill( Data->DataFrame->paletteBackgroundColor() );
		Data->Image->setPixmap( *Thumbnail );
		return;
	}

	//Try to load the object's image from disk
	//If no image found, load "no image" image
	//If that isn't found, make it blank.
	QFile file;
	QString fname = "thumb-" + selectedObject->name().lower().replace( QRegExp(" "), QString() ) + ".png";
	if ( KSUtils::openDataFile( file, fname ) ) {
		file.close();
		Thumbnail->load( file.name(), "PNG" );
	} else if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
		file.close();
		Thumbnail->load( file.name(), "PNG" );
	} else {
		Thumbnail->resize( Data->Image->width(), Data->Image->height() );
		Thumbnail->fill( Data->DataFrame->paletteBackgroundColor() );
	}

	Data->Image->setPixmap( *Thumbnail );
}

void DetailDialog::updateThumbnail() {
	ThumbnailPicker tp( selectedObject, *Thumbnail, this, "thumbnaileditor" );
	
	if ( tp.exec() == QDialog::Accepted ) {
		QString fname = locateLocal( "appdata", "thumb-" 
				+ selectedObject->name().lower().replace( QRegExp(" "), QString() ) + ".png" );

		Data->Image->setPixmap( *(tp.image()) );

		//If a real image was set, save it.
		//If the image was unset, delete the old image on disk.
		if ( tp.imageFound() ) {
			Data->Image->pixmap()->save( fname, "PNG" );
			*Thumbnail = *(Data->Image->pixmap());
		} else {
			QFile f;
			f.setName( fname );
			f.remove();
		}
	}
}

DataWidget::DataWidget( QWidget *p )
{
	setupUi(p);

	//Modify colors
	Names->setPaletteBackgroundColor( p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	Names->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::HighlightedText ) );
	DataFrame->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	Type->setPalette( p->palette() );
	Constellation->setPalette( p->palette() );
	Magnitude->setPalette( p->palette() );
	Distance->setPalette( p->palette() );
	AngSize->setPalette( p->palette() );
	InLabel->setPalette( p->palette() );
	MagLabel->setPalette( p->palette() );
	DistanceLabel->setPalette( p->palette() );
	AngSizeLabel->setPalette( p->palette() );
}

PositionWidget::PositionWidget( QWidget *p )
{
	setupUi(p);

	//Modify colors
	CoordTitle->setPaletteBackgroundColor( p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	CoordTitle->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::HighlightedText ) );
	CoordFrame->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	RSTTitle->setPaletteBackgroundColor( p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	RSTTitle->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::HighlightedText ) );
	RSTFrame->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	RA->setPalette( p->palette() );
	Dec->setPalette( p->palette() );
	Az->setPalette( p->palette() );
	Alt->setPalette( p->palette() );
	HA->setPalette( p->palette() );
	Airmass->setPalette( p->palette() );
	TimeRise->setPalette( p->palette() );
	TimeTransit->setPalette( p->palette() );
	TimeSet->setPalette( p->palette() );
	AzRise->setPalette( p->palette() );
	AltTransit->setPalette( p->palette() );
	AzSet->setPalette( p->palette() );
	RALabel->setPalette( p->palette() );
	DecLabel->setPalette( p->palette() );
	AzLabel->setPalette( p->palette() );
	AltLabel->setPalette( p->palette() );
	HALabel->setPalette( p->palette() );
	AirmassLabel->setPalette( p->palette() );
	TimeRiseLabel->setPalette( p->palette() );
	TimeTransitLabel->setPalette( p->palette() );
	TimeSetLabel->setPalette( p->palette() );
	AzRiseLabel->setPalette( p->palette() );
	AltTransitLabel->setPalette( p->palette() );
	AzSetLabel->setPalette( p->palette() );
}

LinksWidget::LinksWidget( QWidget *p ) 
{
	setupUi(p);

	//Modify colors
	InfoTitle->setPaletteBackgroundColor( p->palette().color( QPalette::Active, QColorGroup::Text ) );
	InfoTitle->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::Base ) );
	ImagesTitle->setPaletteBackgroundColor( p->palette().color( QPalette::Active, QColorGroup::Text ) );
	ImagesTitle->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::Base ) );

	QPalette plt = p->palette();
	plt.setColor( QPalette::Active, QColorGroup::Dark, p->palette().color( QPalette::Active, QColorGroup::Highlight ) );
	InfoList->setPalette( plt );
	ImagesList->setPalette( plt );
}

DatabaseWidget::DatabaseWidget( QWidget *p )
{
	setupUi(p);
}

LogWidget::LogWidget( QWidget *p )
{
	setupUi(p);

	//Modify colors
	LogTitle->setPaletteBackgroundColor( p->palette().color( QPalette::Active, QColorGroup::Text ) );
	LogTitle->setPaletteForegroundColor( p->palette().color( QPalette::Active, QColorGroup::Base ) );
}

#include "detaildialog.moc"
