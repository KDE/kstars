#include <klocale.h>
/****************************************************************************
** Form implementation generated from reading ui file './indiconf.ui'
**
** Created: Sat Jan 31 22:41:40 2004
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "indiconf.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/*
 *  Constructs a INDIConf as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
INDIConf::INDIConf( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "INDIConf" );
    setSizeGripEnabled( TRUE );
    INDIConfLayout = new QGridLayout( this, 1, 1, 11, 6, "INDIConfLayout"); 

    Layout1 = new QHBoxLayout( 0, 0, 6, "Layout1"); 
    QSpacerItem* spacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout1->addItem( spacer );

    buttonOk = new QPushButton( this, "buttonOk" );
    buttonOk->setAutoDefault( TRUE );
    buttonOk->setDefault( TRUE );
    Layout1->addWidget( buttonOk );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setAutoDefault( TRUE );
    Layout1->addWidget( buttonCancel );

    INDIConfLayout->addMultiCellLayout( Layout1, 1, 1, 0, 1 );

    displayGroup = new QGroupBox( this, "displayGroup" );
    displayGroup->setColumnLayout(0, Qt::Vertical );
    displayGroup->layout()->setSpacing( 6 );
    displayGroup->layout()->setMargin( 11 );
    displayGroupLayout = new QVBoxLayout( displayGroup->layout() );
    displayGroupLayout->setAlignment( Qt::AlignTop );

    layout2 = new QVBoxLayout( 0, 0, 6, "layout2"); 

    crosshairCheck = new QCheckBox( displayGroup, "crosshairCheck" );
    crosshairCheck->setChecked( TRUE );
    layout2->addWidget( crosshairCheck );

    messagesCheck = new QCheckBox( displayGroup, "messagesCheck" );
    messagesCheck->setChecked( TRUE );
    layout2->addWidget( messagesCheck );
    displayGroupLayout->addLayout( layout2 );

    INDIConfLayout->addWidget( displayGroup, 0, 1 );

    autoGroup = new QGroupBox( this, "autoGroup" );
    autoGroup->setColumnLayout(0, Qt::Vertical );
    autoGroup->layout()->setSpacing( 6 );
    autoGroup->layout()->setMargin( 11 );
    autoGroupLayout = new QVBoxLayout( autoGroup->layout() );
    autoGroupLayout->setAlignment( Qt::AlignTop );

    layout3 = new QVBoxLayout( 0, 0, 6, "layout3"); 

    timeCheck = new QCheckBox( autoGroup, "timeCheck" );
    timeCheck->setChecked( FALSE );
    layout3->addWidget( timeCheck );

    GeoCheck = new QCheckBox( autoGroup, "GeoCheck" );
    GeoCheck->setChecked( FALSE );
    layout3->addWidget( GeoCheck );
    autoGroupLayout->addLayout( layout3 );

    INDIConfLayout->addWidget( autoGroup, 0, 0 );
    languageChange();
    resize( QSize(452, 172).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( buttonOk, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );

    // tab order
    setTabOrder( timeCheck, GeoCheck );
    setTabOrder( GeoCheck, crosshairCheck );
    setTabOrder( crosshairCheck, messagesCheck );
    setTabOrder( messagesCheck, buttonOk );
    setTabOrder( buttonOk, buttonCancel );
}

/*
 *  Destroys the object and frees any allocated resources
 */
INDIConf::~INDIConf()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void INDIConf::languageChange()
{
    setCaption( tr2i18n( "Configure INDI" ) );
    buttonOk->setText( tr2i18n( "&OK" ) );
    buttonCancel->setText( tr2i18n( "&Cancel" ) );
    displayGroup->setTitle( tr2i18n( "Display" ) );
    crosshairCheck->setText( tr2i18n( "Device target crosshair" ) );
    messagesCheck->setText( tr2i18n( "INDI messages in status bar" ) );
    autoGroup->setTitle( tr2i18n( "Automatic Device Updates" ) );
    timeCheck->setText( tr2i18n( "Time" ) );
    GeoCheck->setText( tr2i18n( "Geographic location" ) );
}

#include "indiconf.moc"
