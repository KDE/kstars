/***************************************************************************
                          finddialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTimer>

#include <kmessagebox.h>

#include "finddialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skyobject.h"

static int AstroTypeRole = Qt::UserRole + 1;

class AstroFindProxyModel : public QSortFilterProxyModel {
	public:
		AstroFindProxyModel( QObject *parent = 0 );
		void setObjectType( int type );

	protected:
		virtual bool filterAcceptsRow( int source_row, const QModelIndex& source_parent ) const;

	private:
		int objectType;
};

AstroFindProxyModel::AstroFindProxyModel( QObject *parent ) : QSortFilterProxyModel( parent ) {
	setFilterCaseSensitivity( Qt::CaseInsensitive );
	setDynamicSortFilter( true );
	objectType = 0;
}

void AstroFindProxyModel::setObjectType( int type ) {
	objectType = type;
	// force the proxy model to update
	setFilterFixedString( filterRegExp().pattern() );
}

bool AstroFindProxyModel::filterAcceptsRow( int source_row, const QModelIndex& source_parent ) const {
	bool accept = true;
	if ( objectType != 0 ) {
		QVariant atype = source_parent.data( AstroTypeRole );
		accept = atype.type() == QVariant::Int ? ( atype.toInt() == objectType ) : false;
	}
	if ( accept ) {
		accept = QSortFilterProxyModel::filterAcceptsRow( source_row, source_parent );
	}
	return accept;
}


FindDialogUI::FindDialogUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );

	FilterType->addItem( i18n ("Any") );
	FilterType->addItem( i18n ("Stars") );
	FilterType->addItem( i18n ("Solar System") );
	FilterType->addItem( i18n ("Open Clusters") );
	FilterType->addItem( i18n ("Glob. Clusters") );
	FilterType->addItem( i18n ("Gas. Nebulae") );
	FilterType->addItem( i18n ("Plan. Nebulae") );
	FilterType->addItem( i18n ("Galaxies") );
	FilterType->addItem( i18n ("Comets") );
	FilterType->addItem( i18n ("Asteroids") );
	FilterType->addItem( i18n ("Constellations") );

	SearchList->setMinimumWidth( 256 );
	SearchList->setMinimumHeight( 320 );
}

FindDialog::FindDialog( QWidget* parent )
    : KDialog( parent ), currentitem(0), timer(0)
{
	ui = new FindDialogUI( this );
	setMainWidget( ui );
        setCaption( i18n( "Find Object" ) );
        setButtons( KDialog::Ok|KDialog::Cancel );

	sortModel = new AstroFindProxyModel( ui->SearchList );

//Connect signals to slots
//	connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( enqueueSearch() ) );
	connect( ui->SearchBox, SIGNAL( returnPressed() ), SLOT( slotOk() ) );
	connect( ui->FilterType, SIGNAL( activated( int ) ), this, SLOT( setFilter( int ) ) );
	connect( ui->SearchList, SIGNAL( doubleClicked( const QModelIndex & ) ), SLOT( slotOk() ) );

	connect(this,SIGNAL(okClicked()),this,SLOT(slotOk()));
	// first create and paint dialog and then load list
	QTimer::singleShot(0, this, SLOT( init() ));
}

FindDialog::~FindDialog() {
}

void FindDialog::init() {
	ui->SearchBox->clear();
	ui->FilterType->setCurrentIndex(0);  // show all types of objects

	KStars *p = (KStars *)parent();
	// FIXME instead of a simple QStringListModel, there should be a custom
	// implementation of QAbstractListModel, able to return also the
	// id for the AstroTypeRole custom role.
	QStringListModel *dataModel = new QStringListModel( sortModel );
	dataModel->setStringList( p->data()->skyComposite()->objectNames() );
	sortModel->setSourceModel( dataModel );
	sortModel->sort( 0 );
	ui->SearchList->setModel( sortModel );
	updateSelection();
}

void FindDialog::filter() {  //Filter the list of names with the string in the SearchBox
	sortModel->setFilterFixedString( ui->SearchBox->text() );
	updateSelection();
}

void FindDialog::updateSelection() {
	if ( sortModel->rowCount() <= 0 ) {
		currentitem = 0;
		button( Ok )->setEnabled( false );
		return;
	}

	QModelIndex first = sortModel->index( 0, sortModel->filterKeyColumn(), QModelIndex() );
	if ( first.isValid() )
		ui->SearchList->selectionModel()->select( first, QItemSelectionModel::ClearAndSelect );

	KStars *p = (KStars *)parent();
	QString objName = first.data().toString();
	currentitem = p->data()->skyComposite()->findByName( objName );
	button( Ok )->setEnabled( true );
}

void FindDialog::setFilter( int f ) {
	if ( timer ) {
		timer->stop();
	}
	sortModel->setObjectType( f );
	updateSelection();
}

void FindDialog::enqueueSearch() {
	if ( timer ) {
		timer->stop();
	} else {
		timer = new QTimer( this );
		timer->setSingleShot( true );
		connect( timer, SIGNAL( timeout() ), this, SLOT( filter() ) );
	}
	timer->start( 500 );
}

void FindDialog::slotOk() {
	//If no valid object selected, show a sorry-box.  Otherwise, emit accept()
	if ( currentItem() == 0 ) {
		QString message = i18n( "No object named %1 found.", ui->SearchBox->text() );
		KMessageBox::sorry( 0, message, i18n( "Bad object name" ) );
	} else {
		accept();
	}
}

void FindDialog::keyPressEvent( QKeyEvent *e ) {
	switch( e->key() ) {
#if 0
		case Qt::Key_Down :
			if ( ui->SearchList->currentRow() < ((int) ui->SearchList->count()) - 1 )
				ui->SearchList->setCurrentRow( ui->SearchList->currentRow() + 1 );
			break;

		case Qt::Key_Up :
			if ( ui->SearchList->currentRow() )
				ui->SearchList->setCurrentRow( ui->SearchList->currentRow() - 1 );
			break;

#endif
		case Qt::Key_Escape :
			reject();
			break;

	}
}

#include "finddialog.moc"
