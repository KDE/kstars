#include <klocale.h>
/****************************************************************************
** Form implementation generated from reading ui file './indihostconf.ui'
**
** Created: Sat Jan 31 22:41:43 2004
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "indihostconf.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/*
 *  Constructs a INDIHostConf as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
INDIHostConf::INDIHostConf( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "INDIHostConf" );
    setSizeGripEnabled( TRUE );
    INDIHostConfLayout = new QVBoxLayout( this, 11, 6, "INDIHostConfLayout"); 

    layout7 = new QHBoxLayout( 0, 0, 6, "layout7"); 

    layout6 = new QVBoxLayout( 0, 0, 6, "layout6"); 

    textLabel1_3 = new QLabel( this, "textLabel1_3" );
    layout6->addWidget( textLabel1_3 );

    textLabel1 = new QLabel( this, "textLabel1" );
    layout6->addWidget( textLabel1 );

    textLabel1_2 = new QLabel( this, "textLabel1_2" );
    layout6->addWidget( textLabel1_2 );
    layout7->addLayout( layout6 );

    layout5 = new QVBoxLayout( 0, 0, 6, "layout5"); 

    nameIN = new QLineEdit( this, "nameIN" );
    layout5->addWidget( nameIN );

    hostname = new QLineEdit( this, "hostname" );
    layout5->addWidget( hostname );

    portnumber = new QLineEdit( this, "portnumber" );
    layout5->addWidget( portnumber );
    layout7->addLayout( layout5 );
    INDIHostConfLayout->addLayout( layout7 );
    QSpacerItem* spacer = new QSpacerItem( 30, 35, QSizePolicy::Minimum, QSizePolicy::Expanding );
    INDIHostConfLayout->addItem( spacer );

    Layout1 = new QHBoxLayout( 0, 0, 6, "Layout1"); 
    QSpacerItem* spacer_2 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout1->addItem( spacer_2 );

    buttonOk = new QPushButton( this, "buttonOk" );
    buttonOk->setAutoDefault( TRUE );
    buttonOk->setDefault( TRUE );
    Layout1->addWidget( buttonOk );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setAutoDefault( TRUE );
    Layout1->addWidget( buttonCancel );
    INDIHostConfLayout->addLayout( Layout1 );
    languageChange();
    resize( QSize(437, 178).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( buttonOk, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );

    // tab order
    setTabOrder( nameIN, hostname );
    setTabOrder( hostname, portnumber );
    setTabOrder( portnumber, buttonOk );
    setTabOrder( buttonOk, buttonCancel );
}

/*
 *  Destroys the object and frees any allocated resources
 */
INDIHostConf::~INDIHostConf()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void INDIHostConf::languageChange()
{
    setCaption( tr2i18n( "INDIHostConf" ) );
    textLabel1_3->setText( tr2i18n( "Name:" ) );
    textLabel1->setText( tr2i18n( "Host:" ) );
    textLabel1_2->setText( tr2i18n( "Port:" ) );
    buttonOk->setText( tr2i18n( "&OK" ) );
    buttonCancel->setText( tr2i18n( "&Cancel" ) );
}

#include "indihostconf.moc"
