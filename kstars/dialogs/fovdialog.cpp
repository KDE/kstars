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
#include <QDebug>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include <KActionCollection>
#include <KLocalizedString>
#include <kcolorbutton.h>
#include <KLocalizedString>
#include <kmessagebox.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "widgets/fovwidget.h"

// This is needed to make FOV work with QVariant
Q_DECLARE_METATYPE(FOV*)

int FOVDialog::fovID = -1;

namespace {
    // Try to convert text in KLine edit to double
    inline double textToDouble(const QLineEdit* edit, bool* ok = 0)
    {
        return edit->text().replace( QLocale().decimalPoint(), "." ).toDouble(ok);
    }

    // Extract FOV from QListWidget. No checking is done
    FOV* getFOV(QListWidgetItem* item)
    {
        return item->data(Qt::UserRole).value<FOV*>();
    }

    // Convert double to QString 
    QString toString(double x, int precision = 2)
    {
        return QString::number(x, 'f', precision).replace( '.', QLocale().decimalPoint() );
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
    QDialog( p )
{
    // Register FOV* data type
    if( fovID == -1 )
        fovID = qRegisterMetaType<FOV*>("FOV*");
    fov = new FOVDialogUI( this );

    setWindowTitle( xi18n( "Set FOV Indicator" ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fov);
    setLayout(mainLayout);

    //FIXME Need porting to KF5
    //setMainWidget( fov );
    //setButtons( QDialog::Ok | QDialog::Cancel );
    
    connect( fov->FOVListBox,   SIGNAL( currentRowChanged( int ) ), SLOT( slotSelect( int ) ) );
    connect( fov->NewButton,    SIGNAL( clicked() ), SLOT( slotNewFOV() ) );
    connect( fov->EditButton,   SIGNAL( clicked() ), SLOT( slotEditFOV() ) );
    connect( fov->RemoveButton, SIGNAL( clicked() ), SLOT( slotRemoveFOV() ) );

    // Read list of FOVs and for each FOV create listbox entry, which stores it.
    foreach(FOV* f, FOV::readFOVs() ) {
        addListWidget(f);
    }
}

FOVDialog::~FOVDialog()
{
    // Delete FOVs 
    for(int i = 0; i < fov->FOVListBox->count(); i++) {
        delete getFOV( fov->FOVListBox->item(i) );
    }
}

QListWidgetItem* FOVDialog::addListWidget(FOV* f)
{
    QListWidgetItem* item = new QListWidgetItem( f->name(), fov->FOVListBox );
    item->setData( Qt::UserRole, QVariant::fromValue<FOV*>(f) );
    return item;
}

void FOVDialog::writeFOVList() {
    QList<FOV*> fovs;
    for(int i = 0; i < fov->FOVListBox->count(); i++) {
        fovs << getFOV( fov->FOVListBox->item(i) );
    }
    FOV::writeFOVs(fovs);
}

void FOVDialog::slotSelect( int irow ) {
    bool enable = irow >= 0;   
    fov->RemoveButton->setEnabled( enable );
    fov->EditButton->setEnabled( enable);
    if( enable ) {
        //paint dialog with selected FOV symbol
        fov->ViewBox->setFOV( getFOV( fov->FOVListBox->currentItem() ) );
        fov->ViewBox->update();
    }
}

void FOVDialog::slotNewFOV() {
    QPointer<NewFOV> newfdlg = new NewFOV( this );
    if ( newfdlg->exec() == QDialog::Accepted ) {
        FOV *newfov = new FOV( newfdlg->getFOV() );
        addListWidget( newfov );
        fov->FOVListBox->setCurrentRow( fov->FOVListBox->count() -1 );
    }
    delete newfdlg;
}

void FOVDialog::slotEditFOV() {
    //Preload current values
    QListWidgetItem* item = fov->FOVListBox->currentItem();
    if( item == 0 )
        return;
    FOV *f = item->data(Qt::UserRole).value<FOV*>();

    // Create dialog 
    QPointer<NewFOV> newfdlg = new NewFOV( this, f );
    if ( newfdlg->exec() == QDialog::Accepted ) {
        // Overwrite FOV
        *f = newfdlg->getFOV();
        fov->ViewBox->update();
    }
    delete newfdlg;
}

void FOVDialog::slotRemoveFOV() {
    int i = fov->FOVListBox->currentRow();
    if( i >= 0 ) {
        QListWidgetItem* item = fov->FOVListBox->takeItem(i);
        delete getFOV(item);
        delete item;
    }
}

//-------------NewFOV------------------//

NewFOV::NewFOV( QWidget *parent, const FOV* fov ) :
    QDialog( parent ), f()
{
    ui = new NewFOVUI( this );

    setWindowTitle( xi18n( "New FOV Indicator" ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    //FIXME Need porting to KF5
    //setMainWidget( ui );
    //setButtons( QDialog::Ok|QDialog::Cancel );

    // Initialize FOV if required
    if( fov != 0 ) {
        f = *fov;
        ui->FOVName->setText( f.name() );
        ui->FOVEditX->setText( toString(f.sizeX()) );
        ui->FOVEditY->setText( toString(f.sizeY()) );
        ui->FOVEditOffsetX->setText( toString(f.offsetX()));
        ui->FOVEditOffsetY->setText( toString(f.offsetY()));
        ui->FOVEditRotation->setText(toString(f.rotation()));
        ui->ColorButton->setColor( QColor( f.color() ) );
        ui->ShapeBox->setCurrentIndex( f.shape() );

        ui->ViewBox->setFOV( &f );
        ui->ViewBox->update();
    }

    connect( ui->FOVName,     SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditX,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditY,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditOffsetX,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditOffsetY,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->FOVEditRotation,    SIGNAL( textChanged( const QString & ) ), SLOT( slotUpdateFOV() ) );
    connect( ui->ColorButton, SIGNAL( changed( const QColor & ) ),      SLOT( slotUpdateFOV() ) );
    connect( ui->ShapeBox,    SIGNAL( activated( int ) ),               SLOT( slotUpdateFOV() ) );
    connect( ui->ComputeEyeFOV,       SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeCameraFOV,    SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeHPBW,         SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeBinocularFOV, SIGNAL( clicked() ), SLOT( slotComputeFOV() ) );
    connect( ui->ComputeTLengthFromFNum1, SIGNAL( clicked() ), SLOT( slotComputeTelescopeFL() ) );
    connect( ui->ComputeTLengthFromFNum2, SIGNAL( clicked() ), SLOT( slotComputeTelescopeFL() ) );

    // Populate eyepiece AFOV options. The userData field contains the apparent FOV associated with that option
    ui->EyepieceAFOV->insertItem( 0, xi18nc("Specify the apparent field of view (AFOV) manually", "Specify AFOV"), -1 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Ramsden (Typical)"), 30 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Orthoscopic (Typical)"), 45 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Ploessl (Typical)"), 50 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Erfle (Typical)"), 60 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Radian"), 60 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Baader Hyperion"), 68 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Panoptic"), 68 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Delos"), 72 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Meade UWA"), 82 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Nagler"), 82 );
    ui->EyepieceAFOV->addItem( xi18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Ethos (Typical)"), 100 );

    connect( ui->EyepieceAFOV, SIGNAL( currentIndexChanged( int ) ), SLOT( slotEyepieceAFOVChanged( int ) ) );

    ui->LinearFOVDistance->insertItem( 0, xi18n( "1000 yards" ) );
    ui->LinearFOVDistance->insertItem( 1, xi18n( "1000 meters" ) );
    connect( ui->LinearFOVDistance, SIGNAL( currentIndexChanged( int ) ), SLOT( slotBinocularFOVDistanceChanged( int ) ) );

    slotUpdateFOV();
}

void NewFOV::slotBinocularFOVDistanceChanged( int index ) {
    QString text = (index == 0  ? xi18n("feet") : xi18n("meters"));
    ui->LabelUnits->setText( text );
}

void NewFOV::slotUpdateFOV() {
    bool okX, okY;
    f.setName( ui->FOVName->text() );
    float sizeX = textToDouble(ui->FOVEditX, &okX);
    float sizeY = textToDouble(ui->FOVEditY, &okY);
    if ( okX && okY )
        f.setSize( sizeX, sizeY );

    float xoffset = textToDouble(ui->FOVEditOffsetX, &okX);
    float yoffset = textToDouble(ui->FOVEditOffsetY, &okY);
    if (okX && okY)
        f.setOffset(xoffset, yoffset);

    float rot = textToDouble(ui->FOVEditRotation, &okX);
    if (okX)
        f.setRotation(rot);

    f.setShape( ui->ShapeBox->currentIndex() );
    f.setColor( ui->ColorButton->color().name() );

    //FIXME Need porting to KF5
    //enableButtonOk( !f.name().isEmpty() && okX && okY );
    
    ui->ViewBox->setFOV( &f );
    ui->ViewBox->update();
}

void NewFOV::slotComputeFOV() {
    if ( sender() == ui->ComputeEyeFOV && ui->TLength1->value() > 0.0 ) {
        ui->FOVEditX->setText( toString(60.0 * ui->EyeFOV->value() * ui->EyeLength->value() / ui->TLength1->value()) );
        ui->FOVEditY->setText( ui->FOVEditX->text() );
    }
    else if ( sender() == ui->ComputeCameraFOV && ui->TLength2->value() > 0.0 ) {
        double sx = (double) ui->ChipSize->value() * 3438.0 / ui->TLength2->value();
        const double aspectratio = 3.0/2.0; // Use the default aspect ratio for DSLRs / Film (i.e. 3:2)
        ui->FOVEditX->setText( toString(sx) );
        ui->FOVEditY->setText( toString(sx / aspectratio) );
    }
    else if ( sender() == ui->ComputeHPBW && ui->RTDiameter->value() > 0.0 && ui->WaveLength->value() > 0.0 ) {
        ui->FOVEditX->setText( toString(34.34 * 1.2 * ui->WaveLength->value() / ui->RTDiameter->value()) );
        // Beam width for an antenna is usually a circle on the sky.
        ui->ShapeBox->setCurrentIndex(4);
        ui->FOVEditY->setText( ui->FOVEditX->text() );
        slotUpdateFOV();
    }
    else if ( sender() == ui->ComputeBinocularFOV && ui->LinearFOV->value() > 0.0 && ui->LinearFOVDistance->currentIndex() >= 0 ) {
        double sx = atan( (double) ui->LinearFOV->value() / ( (ui->LinearFOVDistance->currentIndex() == 0 ) ? 3000.0 : 1000.0 ) ) * 180.0 * 60.0 / dms::PI;
        ui->FOVEditX->setText( toString(sx) );
        ui->FOVEditY->setText( ui->FOVEditX->text() );
    }
}

void NewFOV::slotEyepieceAFOVChanged( int index ) {
    if( index == 0 ) {
        ui->EyeFOV->setEnabled( true );
    }
    else {
        bool ok;
        ui->EyeFOV->setEnabled( false );
        ui->EyeFOV->setValue( ui->EyepieceAFOV->itemData( index ).toFloat( &ok ) );
        Q_ASSERT( ok );
    }
}

void NewFOV::slotComputeTelescopeFL() {
    QObject *whichTab = sender();
    TelescopeFL *telescopeFLDialog = new TelescopeFL( this );
    if ( telescopeFLDialog->exec() == QDialog::Accepted ) {
        Q_ASSERT( whichTab == ui->ComputeTLengthFromFNum1 || whichTab == ui->ComputeTLengthFromFNum2 );
        (( whichTab == ui->ComputeTLengthFromFNum1 ) ? ui->TLength1 : ui->TLength2 )->setValue( telescopeFLDialog->computeFL() );
    }
    delete telescopeFLDialog;
}


//-------------TelescopeFL------------------//

TelescopeFL::TelescopeFL( QWidget *parent ) :
    QDialog( parent ), aperture( 0 ), fNumber( 0 ), apertureUnit( 0 ) {

    setWindowTitle( xi18n( "Telescope Focal Length Calculator" ) );

    //FIXME Need porting to KF5
    //setButtons( QDialog::Ok|QDialog::Cancel );

    QWidget *mainWidget = new QWidget( this );
    QGridLayout *mainLayout = new QGridLayout( mainWidget );
    mainWidget->setLayout( mainLayout );

    //FIXME Need porting to KF5
    //setMainWidget( mainWidget );

    aperture = new QDoubleSpinBox();
    aperture->setRange(0.0, 100000.0);
    aperture->setDecimals(2);
    aperture->setSingleStep(0.1);

    fNumber = new QDoubleSpinBox();
    fNumber->setRange(0.0, 99.9);
    fNumber->setDecimals(2);
    fNumber->setSingleStep(0.1);

    apertureUnit = new QComboBox( this );
    apertureUnit->insertItem( 0, xi18nc("millimeters", "mm") );
    apertureUnit->insertItem( 1, xi18n("inch") );
    mainLayout->addWidget( new QLabel( xi18n("Aperture diameter: "), this ), 0, 0 );
    mainLayout->addWidget( aperture, 0, 1 );
    mainLayout->addWidget( apertureUnit, 0, 2 );
    mainLayout->addWidget( new QLabel( xi18nc("F-Number or F-Ratio of optical system", "F-Number: "), this ), 1, 0 );
    mainLayout->addWidget( fNumber, 1, 1 );
    show();
}

double TelescopeFL::computeFL() const {
    const double inch_to_mm = 25.4; // 1 inch, by definition, is 25.4 mm
    return ( aperture->value() * fNumber->value() * ( ( apertureUnit->currentIndex() == 1 ) ? inch_to_mm : 1.0 ) ); // Focal Length = Aperture * F-Number, by definition of F-Number
}

unsigned int FOVDialog::currentItem() const { return fov->FOVListBox->currentRow(); }

#include "fovdialog.moc"
