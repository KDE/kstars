/***************************************************************************
                          FITS Tab
                             -------------------
    copyright            : (C) 2012 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QClipboard>

#include <KUndoStack>
#include <KLocale>
#include <KMessageBox>
#include <KFileDialog>

#include "Options.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitshistogram.h"
#include "fitsviewer.h"

#include "ui_statform.h"
#include "ui_fitsheaderdialog.h"

FITSTab::FITSTab(FITSViewer *parent) : QWidget()
{
    image      = NULL;
    histogram  = NULL;
    viewer     = parent;

    mDirty     = false;
    undoStack = new KUndoStack(this);
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));
}

FITSTab::~FITSTab()
{
    disconnect(0,0,0);
}

void FITSTab::saveUnsaved()
{

    if( undoStack->isClean() || image->getMode() != FITS_NORMAL)
        return;

    QString caption = xi18n( "Save Changes to FITS?" );
    QString message = xi18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
    int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard() );
    if( ans == KMessageBox::Yes )
        saveFile();
    if( ans == KMessageBox::No )
    {
        undoStack->clear();
        modifyFITSState();
    }
}


void FITSTab::closeEvent(QCloseEvent *ev)
{
    saveUnsaved();
    if( undoStack->isClean() )
        ev->accept();
    else
        ev->ignore();

}

bool FITSTab::loadFITS(const KUrl *imageURL, FITSMode mode, FITSScale filter)
{
    if (image == NULL)
    {
        image = new FITSView(this, mode);
        image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *vlayout = new QVBoxLayout();

        vlayout->addWidget(image);

        setLayout(vlayout);
        connect(image, SIGNAL(newStatus(QString,FITSBar)), this, SIGNAL(newStatus(QString,FITSBar)));
    }

    currentURL = *imageURL;

    bool imageLoad = image->loadFITS(imageURL->url());

    if (imageLoad)
    {
        if (histogram == NULL)
            histogram = new FITSHistogram(this);
        else
            histogram->updateHistogram();

        FITSImage *image_data = image->getImageData();

        image_data->setHistogram(histogram);
        image_data->applyFilter(filter);

        if (filter != FITS_NONE)
            image->rescale(ZOOM_KEEP_LEVEL);

        if (viewer->isStarsMarked())
            image->toggleStars(true);

        image->updateFrame();
    }


    return imageLoad;
}

void FITSTab::modifyFITSState(bool clean)
{
    if (clean)
    {
        if (undoStack->isClean() == false)
            undoStack->setClean();

        mDirty = false;
    }
    else
        mDirty = true;

    emit changeStatus(clean);
}

int FITSTab::saveFITS(const QString &filename)
{
    return image->saveFITS(filename);
}

void FITSTab::copyFITS()
{
    QApplication::clipboard()->setImage( *(image->getDisplayImage()) );
}

void FITSTab::histoFITS()
{
    histogram->show();
}


void FITSTab::statFITS()
{
    QDialog statDialog;
    Ui::statForm stat;
    stat.setupUi(&statDialog);

    FITSImage *image_data = image->getImageData();

    stat.widthOUT->setText(QString::number(image_data->getWidth()));
    stat.heightOUT->setText(QString::number(image_data->getHeight()));
    stat.bitpixOUT->setText(QString::number(image_data->getBPP()));
    stat.maxOUT->setText(QString::number(image_data->getMax()));
    stat.minOUT->setText(QString::number(image_data->getMin()));
    stat.meanOUT->setText(QString::number(image_data->getAverage()));
    stat.stddevOUT->setText(QString::number(image_data->getStdDev(), 'g', 3));
    stat.HFROUT->setText(QString::number(image_data->getHFR(), 'g', 3));

    statDialog.exec();
}

void FITSTab::headerFITS()
{
    QString recordList;
    int nkeys;
    int err_status;
    char err_text[FLEN_STATUS];

    FITSImage *image_data = image->getImageData();

    if ( (err_status = image_data->getFITSRecord(recordList, nkeys)) < 0)
    {
        fits_get_errstatus(err_status, err_text);
        KMessageBox::error(0, xi18n("FITS record error: %1", QString::fromUtf8(err_text)), xi18n("FITS Header"));
        return;
    }

    //FIXME: possible crash! Must use QPointer<...>!
    QDialog fitsHeaderDialog;
    Ui::fitsHeaderDialog header;
    header.setupUi(&fitsHeaderDialog);
    header.tableWidget->setRowCount(nkeys);
    for(int i = 0; i < nkeys; i++)
    {
        QString record = recordList.mid(i*80, 80);
        // I love regexp!
        QStringList properties = record.split(QRegExp("[=/]"));

        QTableWidgetItem* tempItem = new QTableWidgetItem(properties[0].simplified());
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(i, 0, tempItem);

        if (properties.size() > 1)
        {
            tempItem = new QTableWidgetItem(properties[1].simplified());
            tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            header.tableWidget->setItem(i, 1, tempItem);
        }

        if (properties.size() > 2)
        {
            tempItem = new QTableWidgetItem(properties[2].simplified());
            tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            header.tableWidget->setItem(i, 2, tempItem);
        }

    }

    header.tableWidget->resizeColumnsToContents();
    fitsHeaderDialog.exec();

}


void FITSTab::saveFile()
{
    int err_status;
    char err_text[FLEN_STATUS];

    KUrl backupCurrent = currentURL;
    QString currentDir = Options::fitsDir();

    if (currentURL.path().contains("/tmp/"))
        currentURL.clear();

    // If no changes made, return.
    if( mDirty == false && !currentURL.isEmpty())
        return;

    if (currentURL.isEmpty())
    {
        currentURL = KFileDialog::getSaveUrl( currentDir, "*.fits |Flexible Image Transport System");
        // if user presses cancel
        if (currentURL.isEmpty())
        {
            currentURL = backupCurrent;
            return;
        }

        if (currentURL.path().contains('.') == 0)
            currentURL.setPath(currentURL.path() + ".fits");

        if (QFile::exists(currentURL.path()))
        {
            int r = KMessageBox::warningContinueCancel(0,
                        xi18n( "A file named \"%1\" already exists. "
                              "Overwrite it?", currentURL.fileName() ),
                        xi18n( "Overwrite File?" ),
                        KGuiItem(xi18n( "&Overwrite" )) );
            if(r==KMessageBox::Cancel) return;
        }
    }

    if ( currentURL.isValid() )
    {
        if ( (err_status = saveFITS('!' + currentURL.path())) < 0)
        {
            fits_get_errstatus(err_status, err_text);
            // Use KMessageBox or something here
            KMessageBox::error(0, xi18n("FITS file save error: %1",
                                       QString::fromUtf8(err_text)), xi18n("FITS Save"));
            return;
        }

        //statusBar()->changeItem(xi18n("File saved."), 3);

        emit newStatus(xi18n("File saved."), FITS_MESSAGE);
        modifyFITSState();
    } else
    {
        QString message = xi18n( "Invalid URL: %1", currentURL.url() );
        KMessageBox::sorry( 0, message, xi18n( "Invalid URL" ) );
    }
}

void FITSTab::saveFileAs()
{
    currentURL.clear();
    saveFile();
}

void FITSTab::ZoomIn()
{
   image->ZoomIn();
}

void FITSTab::ZoomOut()
{
  image->ZoomOut();
}

void FITSTab::ZoomDefault()
{
  image->ZoomDefault();
}

void FITSTab::tabPositionUpdated()
{
    undoStack->setActive(true);
    emit newStatus(QString("%1%").arg(image->getCurrentZoom()), FITS_ZOOM);
    emit newStatus(QString("%1x%2").arg(image->getImageData()->getWidth()).arg(image->getImageData()->getHeight()), FITS_RESOLUTION);
}
