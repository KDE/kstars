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
    float sx(0.0), sy(0.0);

    f.setFileName( KStandardDirs::locate( "appdata", "fov.dat" ) );

    if ( f.exists() && f.open( QIODevice::ReadOnly ) ) {
        QTextStream stream( &f );
        while ( !stream.atEnd() ) {
            fields = stream.readLine().split( ':' );
            bool ok( false );

            if ( fields.count() == 4 || fields.count() == 5 ) {
                int index = 0;
                nm = fields[index]; // Name
                ++index;
                sx = (float)(fields[index].toDouble( &ok )); // SizeX
                if( !ok )
                    continue;
                ++index;
                if( fields.count() == 5 ) {
                    sy = (float)(fields[index].toDouble( &ok )); // SizeY
                    if( !ok )
                        continue;
                    ++index;
                }
                else
                    sy = sx;
                sh = fields[index].toInt( &ok ); // Shape
                if ( ok )
                    cl = fields[++index]; // Color
            }

            if ( ok ) {
                FOV *newfov = new FOV( nm, sx, sy, sh, cl );
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
    float fovsizeX = newfdlg.ui->FOVEditX->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble();
    float fovsizeY = newfdlg.ui->FOVEditX->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble();

    if ( newfdlg.exec() == QDialog::Accepted ) {
        FOV *newfov = new FOV( newfdlg.ui->FOVName->text(), fovsizeX, fovsizeY,
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
    newfdlg.ui->FOVEditX->setText( QString::number( (double)( f->sizeX() ), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
    newfdlg.ui->FOVEditY->setText( QString::number( (double)( f->sizeY() ), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
    newfdlg.ui->ColorButton->setColor( QColor( f->color() ) );
    newfdlg.ui->ShapeBox->setCurrentIndex( f->shape() );
    newfdlg.slotUpdateFOV();

    if ( newfdlg.exec() == QDialog::Accepted ) {
        double fovsizeX = newfdlg.ui->FOVEditX->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble();
        double fovsizeY = newfdlg.ui->FOVEditY->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble();
        FOV *newfov = new FOV( newfdlg.ui->FOVName->text(), fovsizeX, fovsizeY,
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
    connect( ui->FOVEditX, SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditY, SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
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
    float sizeX = ui->FOVEditX->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble( &sizeOk );
    float sizeY = ui->FOVEditY->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble( &sizeOk );
    if ( sizeOk ) f.setSize( sizeX, sizeY );
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
    if ( sender() == ui->ComputeEyeFOV && ui->TLength1->value() > 0.0 ) {
        ui->FOVEditX->setText( QString::number( (double) ui->EyeFOV->value() * ui->EyeLength->value() / ui->TLength1->value(), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
        ui->FOVEditY->setText( ui->FOVEditX->text() );
    }
    else if ( sender() == ui->ComputeCameraFOV && ui->TLength2->value() > 0.0 ) {
        double sx = (double) ui->ChipSize->value() * 3438.0 / ui->TLength2->value();
        const double aspectratio = 3.0/2.0; // Use the default aspect ratio for DSLRs / Film (i.e. 3:2)
        ui->FOVEditX->setText( QString::number( sx, 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
        ui->FOVEditY->setText( QString::number( sx / aspectratio, 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
    }
    else if ( sender() == ui->ComputeHPBW && ui->RTDiameter->value() > 0.0 && ui->WaveLength->value() > 0.0 ) {
        ui->FOVEditX->setText( QString::number( (double) 34.34 * 1.2 * ui->WaveLength->value() / ui->RTDiameter->value(), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
        // Beam width for an antenna is usually a circle on the sky.
        ui->ShapeBox->setCurrentIndex(4);
        ui->FOVEditY->setText( ui->FOVEditX->text() );
        slotUpdateFOV();
    }
}

unsigned int FOVDialog::currentItem() const { return fov->FOVListBox->currentRow(); }

#include "fovdialog.moc"
