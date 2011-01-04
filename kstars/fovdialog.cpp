/***************************************************************************
                          fovdialog.cpp  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
    copyright            : (C) 2003 by Jason Harris
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

#include <tqlayout.h>
#include <tqfile.h>
#include <tqframe.h>
#include <tqpainter.h>
#include <tqstringlist.h>

#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>
#include <kcolorbutton.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "fovdialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "fovdialogui.h"
#include "newfovui.h"


//---------FOVDialog---------------//
FOVDialog::FOVDialog( TQWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Set FOV Indicator" ), Ok|Cancel, Ok, parent ) {

	ks = (KStars*)parent;

	TQFrame *page = plainPage();
	TQVBoxLayout *vlay = new TQVBoxLayout( page, 0, 0 );
	fov = new FOVDialogUI( page );
	vlay->addWidget( fov );

	connect( fov->FOVListBox, TQT_SIGNAL( currentChanged( TQListBoxItem* ) ), TQT_SLOT( slotSelect( TQListBoxItem* ) ) );
	connect( fov->NewButton, TQT_SIGNAL( clicked() ), TQT_SLOT( slotNewFOV() ) );
	connect( fov->EditButton, TQT_SIGNAL( clicked() ), TQT_SLOT( slotEditFOV() ) );
	connect( fov->RemoveButton, TQT_SIGNAL( clicked() ), TQT_SLOT( slotRemoveFOV() ) );

	FOVList.setAutoDelete( true );
	initList();
}

FOVDialog::~FOVDialog()
{}

void FOVDialog::initList() {
	TQStringList fields;
	TQFile f;

	TQString nm, cl;
	int sh(0);
	float sz(0.0);

	f.setName( locate( "appdata", "fov.dat" ) );

	if ( f.exists() && f.open( IO_ReadOnly ) ) {
		TQListBoxItem *item = 0;

		TQTextStream stream( &f );
		while ( !stream.eof() ) {
			fields = TQStringList::split( ":", stream.readLine() );
			bool ok( false );

			if ( fields.count() == 4 ) {
				nm = fields[0];
				sz = (float)(fields[1].toDouble( &ok ));
				if ( ok ) {
					sh = fields[2].toInt( &ok );
					if ( ok ) {
						cl = fields[3];
					}
				}
			}

			if ( ok ) {
				FOV *newfov = new FOV( nm, sz, sh, cl );
				fov->FOVListBox->insertItem( nm );
				FOVList.append( newfov );

				//Tag item if its name matches the current fov symbol in the main window
				if ( nm == ks->data()->fovSymbol.name() ) item = fov->FOVListBox->item( fov->FOVListBox->count()-1 );
			}
		}

		f.close();

		//preset the listbox selection to the current setting in the main window
		fov->FOVListBox->setSelected( item, true );
		slotSelect( item );
	}
}

void FOVDialog::slotSelect(TQListBoxItem *item) {
	if ( item == 0 ) { //no item selected
		fov->RemoveButton->setEnabled( false );
		fov->EditButton->setEnabled( false );
	} else {
		fov->RemoveButton->setEnabled( true );
		fov->EditButton->setEnabled( true );
	}

	//paint dialog with selected FOV symbol
	update();
}

void FOVDialog::paintEvent( TQPaintEvent * ) {
	//Draw the selected target symbol in the pixmap.
	TQPainter p;
	p.begin( fov->ViewBox );
	p.fillRect( fov->ViewBox->contentsRect(), TQColor( "black" ) );

	if ( fov->FOVListBox->currentItem() >= 0 ) {
		FOV *f = FOVList.at( fov->FOVListBox->currentItem() );
		if ( f->size() > 0 ) {
			f->draw( p, (float)( 0.3*fov->ViewBox->contentsRect().width() ) );
			TQFont smallFont = p.font();
			smallFont.setPointSize( p.font().pointSize() - 2 );
			p.setFont( smallFont );
			p.drawText( 0, fov->ViewBox->contentsRect().height(), i18n("angular size in arcminutes", "%1 arcmin").arg( KGlobal::locale()->formatNumber( f->size() ), 3 ) );
		}
	}

	p.end();
}

void FOVDialog::slotNewFOV() {
	NewFOV newfdlg( this );

	if ( newfdlg.exec() == TQDialog::Accepted ) {
		FOV *newfov = new FOV( newfdlg.ui->FOVName->text(), newfdlg.ui->FOVEdit->text().toDouble(),
				newfdlg.ui->ShapeBox->currentItem(), newfdlg.ui->ColorButton->color().name() );
		fov->FOVListBox->insertItem( newfdlg.ui->FOVName->text() );
		fov->FOVListBox->setSelected( fov->FOVListBox->count() -1, true );
		FOVList.append( newfov );
	}
}

void FOVDialog::slotEditFOV() {
	NewFOV newfdlg( this );
	//Preload current values
	FOV *f = FOVList.at( fov->FOVListBox->currentItem() );

	if (!f)
	 return;

	newfdlg.ui->FOVName->setText( f->name() );
	newfdlg.ui->FOVEdit->setText( KGlobal::locale()->formatNumber( f->size(), 3 ) );
	newfdlg.ui->ColorButton->setColor( TQColor( f->color() ) );
	newfdlg.ui->ShapeBox->setCurrentItem( f->tqshape() );
	newfdlg.slotUpdateFOV();

	if ( newfdlg.exec() == TQDialog::Accepted ) {
		FOV *newfov = new FOV( newfdlg.ui->FOVName->text(), newfdlg.ui->FOVEdit->text().toDouble(),
				newfdlg.ui->ShapeBox->currentItem(), newfdlg.ui->ColorButton->color().name() );
		fov->FOVListBox->changeItem( newfdlg.ui->FOVName->text(), fov->FOVListBox->currentItem() );
		FOVList.tqreplace( fov->FOVListBox->currentItem(), newfov );
	}
}

void FOVDialog::slotRemoveFOV() {
	uint i = fov->FOVListBox->currentItem();
	FOVList.remove( i );
	fov->FOVListBox->removeItem( i );
	if ( i == fov->FOVListBox->count() ) i--; //last item was removed
	fov->FOVListBox->setSelected( i, true );
	fov->FOVListBox->update();
	
	if ( FOVList.isEmpty() ) {
		TQString message( i18n( "You have removed all FOV symbols.  If the list remains empty when you exit this tool, the default symbols will be regenerated." ) );
		KMessageBox::information( 0, message, i18n( "FOV list is empty" ), "dontShowFOVMessage" );
	}

	update();
}

//-------------NewFOV------------------//
NewFOV::NewFOV( TQWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "New FOV Indicator" ), Ok|Cancel, Ok, parent ), f() {
	TQFrame *page = plainPage();
	TQVBoxLayout *vlay = new TQVBoxLayout( page, 0, 0 );
	ui = new NewFOVUI( page );
	vlay->addWidget( ui );

	connect( ui->FOVName, TQT_SIGNAL( textChanged( const TQString & ) ), TQT_SLOT( slotUpdateFOV() ) );
	connect( ui->FOVEdit, TQT_SIGNAL( textChanged( const TQString & ) ), TQT_SLOT( slotUpdateFOV() ) );
	connect( ui->ColorButton, TQT_SIGNAL( changed( const TQColor & ) ), TQT_SLOT( slotUpdateFOV() ) );
	connect( ui->ShapeBox, TQT_SIGNAL( activated( int ) ), TQT_SLOT( slotUpdateFOV() ) );
	connect( ui->ComputeEyeFOV, TQT_SIGNAL( clicked() ), TQT_SLOT( slotComputeFOV() ) );
	connect( ui->ComputeCameraFOV, TQT_SIGNAL( clicked() ), TQT_SLOT( slotComputeFOV() ) );
	connect( ui->ComputeHPBW, TQT_SIGNAL( clicked() ), TQT_SLOT( slotComputeFOV() ) );

	slotUpdateFOV();
}

void NewFOV::slotUpdateFOV() {
	bool sizeOk( false );
	f.setName( ui->FOVName->text() );
	float size = (float)(ui->FOVEdit->text().toDouble( &sizeOk ));
	if ( sizeOk ) f.setSize( size );
	f.setShape( ui->ShapeBox->currentItem() );
	f.setColor( ui->ColorButton->color().name() );

	if ( ! f.name().isEmpty() && sizeOk )
		enableButtonOK( true );
	else
		enableButtonOK( false );

	update();
}

void NewFOV::paintEvent( TQPaintEvent * ) {
	TQPainter p;
	p.begin( ui->ViewBox );
	p.fillRect( ui->ViewBox->contentsRect(), TQColor( "black" ) );
	f.draw( p, (float)( 0.3*ui->ViewBox->contentsRect().width() ) );
	p.drawText( 0, 0, i18n("angular size in arcminutes", "%1 arcmin").arg( KGlobal::locale()->formatNumber( f.size() ), 3 ) );
	p.end();
}

void NewFOV::slotComputeFOV() {
	//DEBUG
	kdDebug() << ":" << sender()->name() << ":" << endl;
	if ( sender()->name() == TQString( "ComputeEyeFOV" ) ) kdDebug() << "A" << endl;
	if ( sender()->name() == TQString( "ComputeEyeFOV" ) && ui->TLength1->value() > 0.0 ) kdDebug() << "B" << endl;

	if ( sender()->name() == TQString( "ComputeEyeFOV" ) && ui->TLength1->value() > 0.0 )
		ui->FOVEdit->setText( KGlobal::locale()->formatNumber( ui->EyeFOV->value() * ui->EyeLength->value() / ui->TLength1->value() ) );
	else if ( sender()->name() == TQString( "ComputeCameraFOV" ) && ui->TLength2->value() > 0.0 )
		ui->FOVEdit->setText( KGlobal::locale()->formatNumber( ui->ChipSize->value() * 3438.0 / ui->TLength2->value() ) );
	else if ( sender()->name() == TQString( "ComputeHPBW" ) && ui->RTDiameter->value() > 0.0 && ui->WaveLength->value() > 0.0 ) {
		ui->FOVEdit->setText( KGlobal::locale()->formatNumber( 34.34 * 1.2 * ui->WaveLength->value() / ui->RTDiameter->value() ) );
		// Beam width for an antenna is usually a circle on the sky.
		ui->ShapeBox->setCurrentItem(4);
		slotUpdateFOV();

	}
}

unsigned int FOVDialog::currentItem() const { return fov->FOVListBox->currentItem(); }

#include "fovdialog.moc"
