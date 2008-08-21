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

#include "fovdialog.h"

#include <QFile>
#include <QFrame>
#include <QPainter>
#include <QTextStream>
#include <QPaintEvent>

#include <kactioncollection.h>
#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>
#include <kcolorbutton.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "widgets/fovwidget.h"

FOVDialogUI::FOVDialogUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

NewFOVUI::NewFOVUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

//---------FOVDialog---------------//
FOVDialog::FOVDialog( KStars *_ks )
        : KDialog( _ks ), ks(_ks)
{
    fov = new FOVDialogUI( this );
    setMainWidget( fov );
    setCaption( i18n( "Set FOV Indicator" ) );
    setButtons( KDialog::Ok|KDialog::Cancel );

    connect( fov->FOVListBox, SIGNAL( currentRowChanged( int ) ), SLOT( slotSelect( int ) ) );
    connect( fov->NewButton, SIGNAL( clicked() ), SLOT( slotNewFOV() ) );
    connect( fov->EditButton, SIGNAL( clicked() ), SLOT( slotEditFOV() ) );
    connect( fov->RemoveButton, SIGNAL( clicked() ), SLOT( slotRemoveFOV() ) );

    //	FOVList.setAutoDelete( true );
    initList();
}

FOVDialog::~FOVDialog()
{}

void FOVDialog::initList() {
    QStringList fields;
    QFile f;

    QString nm, cl;
    int sh(0), irow(0);
    float sz(0.0);

    f.setFileName( KStandardDirs::locate( "appdata", "fov.dat" ) );

    if ( f.exists() && f.open( QIODevice::ReadOnly ) ) {
        QTextStream stream( &f );
        while ( !stream.atEnd() ) {
            fields = stream.readLine().split( ":" );
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
                fov->FOVListBox->addItem( nm );
                FOVList.append( newfov );

                //Tag item if its name matches the current fov symbol in the main window
                if ( nm == ks->data()->fovSymbol.name() ) 
                    irow = fov->FOVListBox->count()-1;
            }
        }

        f.close();

        //preset the listbox selection to the current setting in the main window
        fov->FOVListBox->setCurrentRow( irow );
        slotSelect( irow );
    }
}

void FOVDialog::slotSelect( int irow ) {
    if ( irow < 0 ) { //no item selected
        fov->RemoveButton->setEnabled( false );
        fov->EditButton->setEnabled( false );
    } else {
        fov->RemoveButton->setEnabled( true );
        fov->EditButton->setEnabled( true );
    }

    //paint dialog with selected FOV symbol
    fov->ViewBox->setFOV( FOVList[ currentItem() ] );
    fov->ViewBox->update();
}

void FOVDialog::slotNewFOV() {
    NewFOV newfdlg( this );

    if ( newfdlg.exec() == QDialog::Accepted ) {
        FOV *newfov = new FOV( newfdlg.ui->FOVName->text(), newfdlg.ui->FOVEdit->text().toDouble(),
                               newfdlg.ui->ShapeBox->currentIndex(), newfdlg.ui->ColorButton->color().name() );

        FOVList.append( newfov );

        fov->FOVListBox->addItem( newfdlg.ui->FOVName->text() );
        fov->FOVListBox->setCurrentRow( fov->FOVListBox->count() -1 );
    }
}

void FOVDialog::slotEditFOV() {
    NewFOV newfdlg( this );
    //Preload current values
    FOV *f = FOVList[ currentItem() ];

    if (!f)
        return;

    newfdlg.ui->FOVName->setText( f->name() );
    newfdlg.ui->FOVEdit->setText( KGlobal::locale()->formatNumber( f->size(), 3 ) );
    newfdlg.ui->ColorButton->setColor( QColor( f->color() ) );
    newfdlg.ui->ShapeBox->setCurrentIndex( f->shape() );
    newfdlg.slotUpdateFOV();

    if ( newfdlg.exec() == QDialog::Accepted ) {
        FOV *newfov = new FOV( newfdlg.ui->FOVName->text(), newfdlg.ui->FOVEdit->text().toDouble(),
                               newfdlg.ui->ShapeBox->currentIndex(), newfdlg.ui->ColorButton->color().name() );

        fov->FOVListBox->currentItem()->setText( newfdlg.ui->FOVName->text() );

        //Use the following replacement for QPtrList::replace():
        //(see Qt4 porting guide at doc.trolltech.com)
        delete FOVList[ currentItem() ];
        FOVList[ currentItem() ] = newfov;

        //paint dialog with edited FOV symbol
        fov->ViewBox->setFOV( FOVList[ currentItem() ] );
        fov->ViewBox->update();
    }
}

void FOVDialog::slotRemoveFOV() {
    int i = currentItem();
    delete FOVList.takeAt( i );
    delete fov->FOVListBox->takeItem( i );
    if ( i == fov->FOVListBox->count() ) i--; //last item was removed
    fov->FOVListBox->setCurrentRow( i );
    fov->FOVListBox->update();

    if ( FOVList.isEmpty() ) {
        QString message( i18n( "You have removed all FOV symbols.  If the list remains empty when you exit this tool, the default symbols will be regenerated." ) );
        KMessageBox::information( 0, message, i18n( "FOV list is empty" ), "dontShowFOVMessage" );
    }

    update();
}

//-------------NewFOV------------------//
NewFOV::NewFOV( QWidget *parent )
        : KDialog( parent ), f()
{
    ui = new NewFOVUI( this );
    setMainWidget( ui );
    setCaption( i18n( "New FOV Indicator" ) );
    setButtons( KDialog::Ok|KDialog::Cancel );

    connect( ui->FOVName, SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEdit, SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->ColorButton, SIGNAL( changed( const QColor & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->ShapeBox, SIGNAL( activated( int ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->ComputeEyeFOV, SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeCameraFOV, SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeHPBW, SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );

    slotUpdateFOV();
}

void NewFOV::slotUpdateFOV() {
    bool sizeOk( false );
    f.setName( ui->FOVName->text() );
    float size = (float)(ui->FOVEdit->text().toDouble( &sizeOk ));
    if ( sizeOk ) f.setSize( size );
    f.setShape( ui->ShapeBox->currentIndex() );
    f.setColor( ui->ColorButton->color().name() );

    if ( ! f.name().isEmpty() && sizeOk )
        enableButtonOk( true );
    else
        enableButtonOk( false );

    ui->ViewBox->setFOV( &f );
    ui->ViewBox->update();
}

void NewFOV::slotComputeFOV() {
    KStars *ks = (KStars*)(parent()->parent());

    if ( sender() == ui->ComputeEyeFOV && ui->TLength1->value() > 0.0 )
        ui->FOVEdit->setText( KGlobal::locale()->formatNumber( ui->EyeFOV->value() * ui->EyeLength->value() / ui->TLength1->value() ) );
    else if ( sender() == ui->ComputeCameraFOV && ui->TLength2->value() > 0.0 )
        ui->FOVEdit->setText( KGlobal::locale()->formatNumber( ui->ChipSize->value() * 3438.0 / ui->TLength2->value() ) );
    else if ( sender() == ui->ComputeHPBW && ui->RTDiameter->value() > 0.0 && ui->WaveLength->value() > 0.0 ) {
        ui->FOVEdit->setText( KGlobal::locale()->formatNumber( 34.34 * 1.2 * ui->WaveLength->value() / ui->RTDiameter->value() ) );
        // Beam width for an antenna is usually a circle on the sky.
        ui->ShapeBox->setCurrentIndex(4);
        slotUpdateFOV();

    }
}

unsigned int FOVDialog::currentItem() const { return fov->FOVListBox->currentRow(); }

#include "fovdialog.moc"
