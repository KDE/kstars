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

namespace {
    // Try to convert text in KLine edit to double
    inline double textToDouble(const KLineEdit* edit, bool* ok = 0)
    {
        return edit->text().replace( KGlobal::locale()->decimalSymbol(), "." ).toDouble(ok);
    }
}

FOVDialogUI::FOVDialogUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

NewFOVUI::NewFOVUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

//---------FOVDialog---------------//
FOVDialog::FOVDialog( QWidget* p ) :
    KDialog( p )
{
    fov = new FOVDialogUI( this );
    setMainWidget( fov );
    setCaption( i18n( "Set FOV Indicator" ) );
    setButtons( KDialog::Ok|KDialog::Cancel );

    connect( fov->FOVListBox,   SIGNAL( currentRowChanged( int ) ), SLOT( slotSelect( int ) ) );
    connect( fov->NewButton,    SIGNAL( clicked() ), SLOT( slotNewFOV() ) );
    connect( fov->EditButton,   SIGNAL( clicked() ), SLOT( slotEditFOV() ) );
    connect( fov->RemoveButton, SIGNAL( clicked() ), SLOT( slotRemoveFOV() ) );

    FOVList = FOV::readFOVs();
    foreach(FOV* f, FOVList) {
        fov->FOVListBox->addItem( f->name() );
    }
}

FOVDialog::~FOVDialog()
{}

void FOVDialog::slotSelect( int irow ) {
    bool enable = irow >= 0;   
    fov->RemoveButton->setEnabled( enable );
    fov->EditButton->setEnabled( enable);

    //paint dialog with selected FOV symbol
    fov->ViewBox->setFOV( FOVList[ currentItem() ] );
    fov->ViewBox->update();
}

void FOVDialog::slotNewFOV() {
    QPointer<NewFOV> newfdlg = new NewFOV( this );
    if ( newfdlg->exec() == QDialog::Accepted ) {
        FOV *newfov = new FOV( newfdlg->ui->FOVName->text(),
                               textToDouble( newfdlg->ui->FOVEditX ),
                               textToDouble( newfdlg->ui->FOVEditX ),
                               FOV::intToShape(newfdlg->ui->ShapeBox->currentIndex()),
                               newfdlg->ui->ColorButton->color().name() );

        FOVList.append( newfov );

        fov->FOVListBox->addItem( newfdlg->ui->FOVName->text() );
        fov->FOVListBox->setCurrentRow( fov->FOVListBox->count() -1 );
    }
    delete newfdlg;
}

void FOVDialog::slotEditFOV() {
    //Preload current values
    FOV *f = FOVList[ currentItem() ];

    if (!f)
        return;

    QPointer<NewFOV> newfdlg = new NewFOV( this );
    newfdlg->ui->FOVName->setText( f->name() );
    newfdlg->ui->FOVEditX->setText( QString::number( (double)( f->sizeX() ), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
    newfdlg->ui->FOVEditY->setText( QString::number( (double)( f->sizeY() ), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
    newfdlg->ui->ColorButton->setColor( QColor( f->color() ) );
    newfdlg->ui->ShapeBox->setCurrentIndex( f->shape() );
    newfdlg->slotUpdateFOV();

    if ( newfdlg->exec() == QDialog::Accepted ) {
        FOV *newfov = new FOV( newfdlg->ui->FOVName->text(), 
                               textToDouble( newfdlg->ui->FOVEditX ),
                               textToDouble( newfdlg->ui->FOVEditY ),
                               FOV::intToShape(newfdlg->ui->ShapeBox->currentIndex()),
                               newfdlg->ui->ColorButton->color().name() );

        fov->FOVListBox->currentItem()->setText( newfdlg->ui->FOVName->text() );

        //Use the following replacement for QPtrList::replace():
        //(see Qt4 porting guide at doc.trolltech.com)
        delete FOVList[ currentItem() ];
        FOVList[ currentItem() ] = newfov;

        //paint dialog with edited FOV symbol
        fov->ViewBox->setFOV( FOVList[ currentItem() ] );
        fov->ViewBox->update();
    }
    delete newfdlg;
}

void FOVDialog::slotRemoveFOV() {
    int i = currentItem();
    delete FOVList.takeAt( i );
    delete fov->FOVListBox->takeItem( i );
    if ( i == fov->FOVListBox->count() ) i--; //last item was removed
    fov->FOVListBox->setCurrentRow( i );
    fov->FOVListBox->update();
    
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

    connect( ui->FOVName,     SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditX,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditY,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->ColorButton, SIGNAL( changed( const QColor & ) ),      SLOT( slotUpdateFOV() ) );
    connect( ui->ShapeBox,    SIGNAL( activated( int ) ),               SLOT( slotUpdateFOV() ) );
    connect( ui->ComputeEyeFOV,       SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeCameraFOV,    SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeHPBW,         SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeBinocularFOV, SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );

    ui->LinearFOVDistance->insertItem( 0, i18n( "1000 yards" ) );
    ui->LinearFOVDistance->insertItem( 1, i18n( "1000 meters" ) );
    connect( ui->LinearFOVDistance, SIGNAL( currentIndexChanged( int ) ), SLOT( slotBinocularFOVDistanceChanged( int ) ) );

    slotUpdateFOV();
}

void NewFOV::slotBinocularFOVDistanceChanged( int index ) {
    if( index == 0 )
        ui->LabelUnits->setText( i18n( "feet" ) );
    else
        ui->LabelUnits->setText( i18n( "meters" ) );
}

void NewFOV::slotUpdateFOV() {
    bool okX, okY;
    f.setName( ui->FOVName->text() );
    float sizeX = textToDouble(ui->FOVEditX, &okX);
    float sizeY = textToDouble(ui->FOVEditY, &okY);
    if ( okX && okY )
        f.setSize( sizeX, sizeY );
    f.setShape( ui->ShapeBox->currentIndex() );
    f.setColor( ui->ColorButton->color().name() );

    enableButtonOk( !f.name().isEmpty() && okX && okY );
    
    ui->ViewBox->setFOV( &f );
    ui->ViewBox->update();
}

void NewFOV::slotComputeFOV() {
    if ( sender() == ui->ComputeEyeFOV && ui->TLength1->value() > 0.0 ) {
        ui->FOVEditX->setText( QString::number( 60.0 * ui->EyeFOV->value() * ui->EyeLength->value() / ui->TLength1->value(), 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
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
    else if ( sender() == ui->ComputeBinocularFOV && ui->LinearFOV->value() > 0.0 && ui->LinearFOVDistance->currentIndex() >= 0 ) {
        double sx = atan( (double) ui->LinearFOV->value() / ( (ui->LinearFOVDistance->currentIndex() == 0 ) ? 3000.0 : 1000.0 ) ) * 180.0 * 60.0 / dms::PI;
        ui->FOVEditX->setText( QString::number( sx, 'f', 2 ).replace( '.', KGlobal::locale()->decimalSymbol() ) );
        ui->FOVEditY->setText( ui->FOVEditX->text() );
    }
}

unsigned int FOVDialog::currentItem() const { return fov->FOVListBox->currentRow(); }

#include "fovdialog.moc"
