/***************************************************************************
                          detaildialog.cpp  -  description
                             -------------------
    begin                : Sun May 5 2002
    copyright            : (C) 2002 by Jason Harris
    				       Jasem Mutlaq
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
#include <qstring.h>

#include <kmessagebox.h>
#include <klistview.h>

#include "geolocation.h"
#include "ksutils.h"
#include "skymap.h"
#include "skyobject.h"
#include "starobject.h"
#include "deepskyobject.h"
#include "kstars.h"

#include "detaildialog.h"

#include <kapplication.h>


DetailDialog::DetailDialog(SkyObject *o, QDateTime lt, GeoLocation *geo,
		QWidget *parent, const char *name ) : KDialogBase( KDialogBase::Tabbed, i18n( "Object Details" ), Close, Close, parent, name ) {

    selectedObject = o;
    ksw = (KStars*) parent;

    createGeneralTab(lt, geo);
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

   userLog = new QTextEdit(logTab, "userLog");
//   userLog->setTextFormat(Qt::RichText);

   if (selectedObject->userLog.isEmpty())
      userLog->setText(i18n("Record here observation logs and/or data on %1.").arg(selectedObject->name()));
   else
      userLog->setText(selectedObject->userLog);

   saveLog = new QPushButton(i18n("Save"), logTab, "Save");

   LOGbuttonSpacer = new QSpacerItem(40, 10, QSizePolicy::Expanding, QSizePolicy::Minimum);
   LOGbuttonLayout = new QHBoxLayout(5, "buttonlayout");
   LOGbuttonLayout->addWidget(saveLog);
   LOGbuttonLayout->addItem(LOGbuttonSpacer);

   logLayout = new QVBoxLayout(logTab, 6, 6, "logLayout");
   logLayout->addWidget(userLog);
   logLayout->addLayout(LOGbuttonLayout);

   connect(saveLog, SIGNAL(clicked()), this, SLOT(saveLogData()));

}
void DetailDialog::createAdvancedTab()
{
  // We don't create a a log tab for an unnamed object or if advinterface file failed loading
  // We also don't need adv dialog for solar system objects.
   if (selectedObject->name() == QString("star") || ksw->data()->ADVtreeList.isEmpty() || selectedObject->type() == SkyObject::PLANET ||selectedObject->type() == SkyObject::COMET || selectedObject->type() == SkyObject::ASTEROID)
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

  Populate(NULL);

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

  for ( ; itList != selectedObject->InfoList.end(); ++itList ) {
         infoList->insertItem(QString(*itTitle));
         itTitle++;
	}

  infoList->setSelected(0, true);

	itList  = selectedObject->ImageList.begin();
	itTitle = selectedObject->ImageTitle.begin();

	for ( ; itList != selectedObject->ImageList.end(); ++itList ) {
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

void DetailDialog::createGeneralTab(QDateTime lt, GeoLocation *geo)
{

	QFrame *generalTab= addPage(i18n("General"));

	ut = lt.addSecs( int( 3600*geo->TZ() ) );
	jd = KSUtils::UTtoJD( ut );

	Coords = new CoordBox( selectedObject, lt, generalTab );
	RiseSet = new RiseSetBox( selectedObject, lt, geo, generalTab );

	StarObject *s;
	DeepSkyObject *dso;

	QString pname, oname;
//arguments to NameBox depend on type of object
	switch ( selectedObject->type() ) {
	case 0: //stars
		s = (StarObject *)selectedObject;
		pname = s->translatedName();
		if ( pname == i18n( "star" ) ) pname = i18n( "Unnamed star" );
		Names = new NameBox( pname, s->gname(),
				i18n( "Spectral type:" ), s->sptype(),
				QString("%1").arg( s->mag() ), generalTab );
//		ProperMotion = new ProperMotionBox( s );
		break;
	case 2: //planets
		//Want to add distance from Earth, Mass, angular size.
		//Perhaps: list of moons

		//the Sun should display type=star, not planet!
		if ( selectedObject->name() == "Sun" ) {
			Names = new NameBox( selectedObject->translatedName(), "", i18n( "Object type:" ),
					i18n("star"), "--", generalTab );
		} else {
			Names = new NameBox( selectedObject->translatedName(), "", i18n( "Object type:" ),
					selectedObject->typeName(), "--", generalTab );
		}
		break;
	case 9:  //comets
	case 10: //asteroids:
		Names = new NameBox( selectedObject->translatedName(), "", i18n( "Object type:" ),
					selectedObject->typeName(), "--", generalTab );
		break;
	default: //deep-sky objects
		dso = (DeepSkyObject *)selectedObject;

		if ( ! dso->longname().isEmpty() ) {
			pname = dso->longname();
			oname = dso->name();
		} else {
			pname = dso->name();
		}
		if ( ! dso->name2().isEmpty() ) oname += ", " + dso->name2();
		if ( dso->ugc() != 0 ) oname += ", UGC " + QString("%1").arg( dso->ugc() );
		if ( dso->pgc() != 0 ) oname += ", PGC " + QString("%1").arg( dso->pgc() );

		Names = new NameBox( pname, oname, i18n( "Object type:" ),
				dso->typeName(), QString("%1").arg( dso->mag() ), generalTab );
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
		QWidget *parent, const char *name )
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
	Mag = new QLabel( mag, this );
	Mag->setAlignment( AlignRight );
	Mag->setFont( boldFont );

//Layout
	hlayType = new QHBoxLayout( 2 );
	hlayMag  = new QHBoxLayout( 2 );
	glay     = new QGridLayout( 2, 2, 10 );
	vlay     = new QVBoxLayout( this, 12 );

	hlayType->addWidget( TypeLabel );
	hlayType->addWidget( Type );
	hlayMag->addWidget( MagLabel );
	hlayMag->addWidget( Mag );

	glay->addWidget( PrimaryName, 0, 0);
	glay->addWidget( OtherNames, 1, 0);
	glay->addLayout( hlayType, 0, 1 );
	glay->addLayout( hlayMag, 1, 1 );
	vlay->addSpacing( 10 );
	vlay->addLayout( glay );
}

//In the printf() statements, the "176" refers to the ASCII code for the degree symbol

DetailDialog::CoordBox::CoordBox( SkyObject *o, QDateTime t, QWidget *parent,
		const char *name ) : QGroupBox( i18n( "Coordinates" ), parent, name ) {

	double epoch = t.date().year() + double( t.date().dayOfYear() )/365.25;
	RALabel = new QLabel( i18n( "RA (%1):" ).arg( epoch, 7, 'f', 2 ), this );
	DecLabel = new QLabel( i18n( "Dec (%1):" ).arg( epoch, 7, 'f', 2 ), this );
	RA = new QLabel( QString().sprintf( "%02dh %02dm %02ds", o->ra()->hour(), o->ra()->minute(), o->ra()->second() ), this );
	Dec = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", o->dec()->degree(), 176, o->dec()->arcmin(), o->dec()->arcsec() ), this );

	AzLabel = new QLabel( i18n( "Azimuth:" ), this );
	AltLabel = new QLabel( i18n( "Altitude:" ), this );
	Az = new QLabel( QString().sprintf( "%02d%c %02d\'%02d\"", o->az()->degree(), 176, o->az()->arcmin(), o->az()->arcsec() ), this );
	Alt = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", o->alt()->degree(), 176, o->alt()->arcmin(), o->alt()->arcsec() ), this );

	QFont boldFont = RA->font();
	boldFont.setWeight( QFont::Bold );
	RA->setFont( boldFont );
	Dec->setFont( boldFont );
	Az->setFont( boldFont );
	Alt->setFont( boldFont );
	RA->setAlignment( AlignRight );
	Dec->setAlignment( AlignRight );
	Az->setAlignment( AlignRight );
	Alt->setAlignment( AlignRight );

//Layouts
	glayCoords = new QGridLayout( 5, 2, 10 );
	vlayMain = new QVBoxLayout( this, 12 );

	glayCoords->addWidget( RALabel, 0, 0 );
	glayCoords->addWidget( DecLabel, 1, 0 );
	glayCoords->addWidget( RA, 0, 1 );
	glayCoords->addWidget( Dec, 1, 1 );
	glayCoords->addItem( new QSpacerItem(20,2), 0, 2 );
	glayCoords->addItem( new QSpacerItem(20,2), 1, 2 );
	glayCoords->addWidget( AzLabel, 0, 3 );
	glayCoords->addWidget( AltLabel, 1, 3 );
	glayCoords->addWidget( Az, 0, 4 );
	glayCoords->addWidget( Alt, 1, 4 );
	vlayMain->addSpacing( 10 );
	vlayMain->addLayout( glayCoords );
}

DetailDialog::RiseSetBox::RiseSetBox( SkyObject *o, QDateTime lt, GeoLocation *geo,
		QWidget *parent, const char *name ) : QGroupBox( i18n( "Rise/Set/Transit" ), parent, name ) {

	QDateTime ut = lt.addSecs( int( -3600*geo->TZ() ) );
	long double jd = KSUtils::UTtoJD( ut );
	QTime rt = o->riseSetTime( jd, geo, true ); //true = use rise time
	dms raz = o->riseSetTimeAz(jd, geo, true ); //true = use rise time
	
	//If transit time is before rise time, use transit time for tomorrow
	QTime tt = o->transitTime( jd, geo );
	dms talt = o->transitAltitude( jd, geo );
	if ( tt < rt ) {
		tt = o->transitTime( jd+1.0, geo );
		talt = o->transitAltitude( jd+1.0, geo );
	}

	//If set time is before rise time, use set time for tomorrow
	QTime st = o->riseSetTime( jd, geo, false ); //false = use set time
	dms saz = o->riseSetTimeAz(jd, geo, false ); //false = use set time
	if ( st < rt ) {
		st = o->riseSetTime( jd+1.0, geo, false ); //false = use set time
		saz = o->riseSetTimeAz(jd+1.0, geo, false ); //false = use set time
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
		RAz = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", raz.degree(), 176, raz.arcmin(), raz.arcsec() ), this );
		SAz = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", saz.degree(), 176, saz.arcmin(), saz.arcsec() ), this );
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
	TAlt = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", talt.degree(), 176, talt.arcmin(), talt.arcsec() ), this );


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

  for ( ; itList != selectedObject->InfoList.end(); ++itList ) {
         infoList->insertItem(QString(*itTitle));
         itTitle++;
	}

  infoList->setSelected(0, true);

	itList  = selectedObject->ImageList.begin();
	itTitle = selectedObject->ImageTitle.begin();

	for ( ; itList != selectedObject->ImageList.end(); ++itList ) {
         imagesList->insertItem(QString(*itTitle));
         itTitle++;
	}

}

void DetailDialog::editLinkDialog()
{
  int type;
  uint i, ObjectIndex;
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
  uint i, ObjectIndex;
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
  
  if (KMessageBox::questionYesNoCancel( 0, i18n("Are you sure you want to remove the %1 link?").arg(currentItemTitle), i18n("Delete confirmation..."))!=KMessageBox::Yes)
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

void DetailDialog::Populate(QListViewItem *parent)
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
       // if [END]
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

  Populate(current);

  ++(*treeIt);

}

void  DetailDialog::viewADVData()
{
   QString link;

   QListViewItem * current = ADVTree->currentItem();

   if (!current)
     return;

   treeIt->toFirst();

   while (treeIt->current())
   {
     if (treeIt->current()->Name == current->text(0))
     {
        if (treeIt->current()->Type == 2)
          break;
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
 QString currentLog = userLog->text();

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
