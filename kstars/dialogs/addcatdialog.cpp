/***************************************************************************
                          addcatdialog.cpp  -  description
                             -------------------
    begin                : Sun Mar 3 2002
    copyright            : (C) 2002 by Jason Harris
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

#include "addcatdialog.h"

#include <QFrame>
#include <QTextStream>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QTemporaryFile>
#include <QUrl>

#include <KColorButton>
#include <KMessageBox>

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skycomponents/catalogcomponent.h"
#include "skycomponents/skymapcomposite.h"

AddCatDialogUI::AddCatDialogUI( QWidget *parent ) : QFrame( parent ) {
    setupUi(this);
}

AddCatDialog::AddCatDialog( KStars *_ks )
        : QDialog( ( QWidget* )_ks )
{
    QDir::setCurrent( QDir::homePath() );
    acd = new AddCatDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(acd);
    setLayout(mainLayout);

    setWindowTitle( i18n( "Import Catalog" ) );

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Help|QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect( acd->DataURL->lineEdit(), SIGNAL( lostFocus() ), this, SLOT( slotShowDataFile() ) );
    connect( acd->DataURL, SIGNAL( urlSelected( const QUrl & ) ),
             this, SLOT( slotShowDataFile() ) );
    connect( acd->PreviewButton, SIGNAL( clicked() ), this, SLOT( slotPreviewCatalog() ) );
    connect( buttonBox->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), this, SLOT( slotCreateCatalog() ) );
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));
    connect(buttonBox, SIGNAL(helpRequested()), this, SLOT(slotHelp()));

    acd->FieldList->addItem( i18n( "ID Number" ) );
    acd->FieldList->addItem( i18n( "Right Ascension" ) );
    acd->FieldList->addItem( i18n( "Declination" ) );
    acd->FieldList->addItem( i18n( "Object Type" ) );

    acd->FieldPool->addItem( i18n( "Common Name" ) );
    acd->FieldPool->addItem( i18n( "Magnitude" ) );
    acd->FieldPool->addItem( i18n( "Flux" ) );
    acd->FieldPool->addItem( i18n( "Major Axis" ) );
    acd->FieldPool->addItem( i18n( "Minor Axis" ) );
    acd->FieldPool->addItem( i18n( "Position Angle" ) );
    acd->FieldPool->addItem( i18n( "Ignore" ) );
}

AddCatDialog::~AddCatDialog(){
}

void AddCatDialog::slotHelp() {
    QString message =
        i18n( "A valid custom catalog file has one line per object, "
              "with the following fields in each line:") + "\n\t" +
        i18n( "1. Type identifier.  Must be one of: 0 (star), 3 (open cluster), 4 (globular cluster), "
              "5 (gaseous nebula), 6 (planetary nebula), 7 (supernova remnant), or 8 (galaxy)" ) + "\n\t" +
        i18n( "2. Right Ascension (floating-point value)" ) + "\n\t" +
        i18n( "3. Declination (floating-point value)" ) + "\n\t" +
        i18n( "4. Magnitude (floating-point value)" ) + "\n\t" +
        i18n( "5. Integrated Flux (floating-point value); frequency and units are set separately in the catalog file." ) + "\n\t" +
        i18n( "6. Spectral type (if type=0); otherwise object's catalog name" ) + "\n\t" +
        i18n( "7. Star name (if type=0); otherwise object's common name. [field 7 is optional]" ) + "\n\n" +

        i18n( "The fields should be separated by whitespace.  In addition, the catalog "
              "may contain comment lines beginning with \'#\'." );

    KMessageBox::information( 0, message, i18n( "Help on custom catalog file format" ) );
}

/* Attempt to parse the catalog data file specified in the DataURL box.
 * We assume the data file has space-separated fields, and that each line has
 * the data fields listed in the Catalog fields list, in the same order.
 * Each data field is parsed as follows:
 *
 * ID number: integer value
 * Right Ascension: colon-delimited hh:mm:ss.s or floating-point value
 * Declination: colon-delimited dd:mm:ss.s or floating-point value
 * Object type: integer value, one of [ 0,1,2,3,4,5,6,7,8 ]
 * Common name: string value (if it contains a space, it *must* be enclosed in quotes!)
 * Magnitude: floating-point value
 * Major axis: floating-point value (length of major axis in arcmin)
 * Minor axis: floating-point value (length of minor axis in arcmin)
 * Position angle: floating-point value (position angle, in degrees)
 */
bool AddCatDialog::validateDataFile() {
    //Create the catalog file contents: first the header
    CatalogContents = writeCatalogHeader();

    // Check if the URL is empty
    if (acd->DataURL->url().isEmpty())
	return false;

    //Next, the data lines (fill from user-specified file)
    QFile dataFile( acd->DataURL->url().toLocalFile() );
    if (dataFile.open( QIODevice::ReadOnly ) ) {
        QTextStream dataStream( &dataFile );
        CatalogContents += dataStream.readAll();

        dataFile.close();
        return true;
    }

    return false;

}

QString AddCatDialog::writeCatalogHeader() {
    QString name = ( acd->CatalogName->text().isEmpty() ? i18n("Custom") : acd->CatalogName->text() );
    QString pre = ( acd->Prefix->text().isEmpty() ? "CC" : acd->Prefix->text() );
    char delimiter = ( acd->CSVButton->isChecked() ? ',' : ' ' );

    QString h = QString("# Delimiter: %1\n").arg( delimiter );
    h += QString("# Name: %1\n").arg( name );
    h += QString("# Prefix: %1\n").arg( pre );
    h += QString("# Color: %1\n").arg( acd->ColorButton->color().name() );
    h += QString("# Epoch: %1\n").arg( acd->Epoch->value() );
    h += QString("# ");

    for ( int i=0; i < acd->FieldList->count(); ++i ) {
        QString f = acd->FieldList->item( i )->text();

        if ( f == i18n( "ID Number" ) ) {
            h += "ID  ";
        } else if ( f == i18n( "Right Ascension" ) ) {
            h += "RA  ";
        } else if ( f == i18n( "Declination" ) ) {
            h += "Dc  ";
        } else if ( f == i18n( "Object Type" ) ) {
            h += "Tp  ";
        } else if ( f == i18n( "Common Name" ) ) {
            h += "Nm  ";
        } else if ( f == i18n( "Magnitude" ) ) {
            h += "Mg  ";
        } else if ( f == i18n( "Flux" ) ) {
            h += "Flux  ";
        } else if ( f == i18n( "Major Axis" ) ) {
            h += "Mj  ";
        } else if ( f == i18n( "Minor Axis" ) ) {
            h += "Mn  ";
        } else if ( f == i18n( "Position Angle" ) ) {
            h += "PA  ";
        } else if ( f == i18n( "Ignore" ) ) {
            h += "Ig  ";
        }
    }

    h += '\n';

    return h;
}

void AddCatDialog::slotShowDataFile() {
    QFile dataFile( acd->DataURL->url().toLocalFile() );
    if ( ! acd->DataURL->url().isEmpty() && dataFile.open( QIODevice::ReadOnly ) ) {
        acd->DataFileBox->clear();
        QTextStream dataStream( &dataFile );
        acd->DataFileBox->addItems( dataStream.readAll().split( '\n' ) );
        dataFile.close();
    }
}

void AddCatDialog::slotPreviewCatalog() {
    if ( validateDataFile() ) {
        KMessageBox::informationList( 0, i18n( "Preview of %1", acd->CatalogName->text() ),
                                      CatalogContents.split( '\n' ), i18n( "Catalog Preview" ) );
    }
}

void AddCatDialog::slotCreateCatalog() {

    if ( validateDataFile() ) {
        //CatalogContents now contains the text for the catalog file,
        //and objList contains the parsed objects

        //Warn user if file exists!
        if ( QFile::exists( acd->CatalogURL->url().toLocalFile() ) )
        {
            QUrl u( acd->CatalogURL->url() );
            int r=KMessageBox::warningContinueCancel( 0,
                    i18n( "A file named \"%1\" already exists. "
                          "Overwrite it?", u.fileName() ),
                    i18n( "Overwrite File?" ),
                    KStandardGuiItem::overwrite() );

            if(r==KMessageBox::Cancel) return;
        }

        QFile OutFile( acd->CatalogURL->url().toLocalFile() );
        if ( ! OutFile.open( QIODevice::WriteOnly ) ) {
            KMessageBox::sorry( 0,
                                i18n( "Could not open the file %1 for writing.", acd->CatalogURL->url().toLocalFile() ),
                                i18n( "Error Opening Output File" ) );
        } else {
            QTextStream outStream( &OutFile );
            outStream << CatalogContents;
            OutFile.close();

            KStarsData::Instance()->skyComposite()->addCustomCatalog(OutFile.fileName(), 0);

            emit QDialog::accept();
            close();
        }
    }
}


