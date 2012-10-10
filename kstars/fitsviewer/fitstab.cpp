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
#include "fitsimage.h"
#include "fitshistogram.h"

#include "ui_statform.h"
#include "ui_fitsheaderdialog.h"

#define JM_INDEX_LIMIT  5

FITSTab::FITSTab() : QWidget()
{

    image      = NULL;
    histogram  = NULL;

    mDirty     = false;
    undoStack = new KUndoStack();
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));

}

void FITSTab::saveUnsaved()
{

    if( undoStack->isClean() )
        return;

    QString caption = i18n( "Save Changes to FITS?" );
    QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
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
        image = new FITSImage(this, mode);
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

        image->setHistogram(histogram);
        image->applyFilter(filter);

        if (histogram->getJMIndex() > JM_INDEX_LIMIT)
        {
            image->findCentroid();
            image->getHFR();
        }

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

    stat.widthOUT->setText(QString::number(image->stats.dim[0]));
    stat.heightOUT->setText(QString::number(image->stats.dim[1]));
    stat.bitpixOUT->setText(QString::number(image->stats.bitpix));
    stat.maxOUT->setText(QString::number(image->stats.max));
    stat.minOUT->setText(QString::number(image->stats.min));
    stat.meanOUT->setText(QString::number(image->stats.average));
    stat.stddevOUT->setText(QString::number(image->stats.stddev, 'g', 3));
    stat.HFROUT->setText(QString::number(image->getHFR(), 'g', 3));

    statDialog.exec();
}

void FITSTab::headerFITS()
{
    QString recordList;
    int nkeys;
    int err_status;
    char err_text[FLEN_STATUS];

    if ( (err_status = image->getFITSRecord(recordList, nkeys)) < 0)
    {
        fits_get_errstatus(err_status, err_text);
        KMessageBox::error(0, i18n("FITS record error: %1", QString::fromUtf8(err_text)), i18n("FITS Header"));
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
                        i18n( "A file named \"%1\" already exists. "
                              "Overwrite it?", currentURL.fileName() ),
                        i18n( "Overwrite File?" ),
                        KGuiItem(i18n( "&Overwrite" )) );
            if(r==KMessageBox::Cancel) return;
        }
    }

    if ( currentURL.isValid() )
    {
        if ( (err_status = saveFITS('!' + currentURL.path())) < 0)
        {
            fits_get_errstatus(err_status, err_text);
            // Use KMessageBox or something here
            KMessageBox::error(0, i18n("FITS file save error: %1",
                                       QString::fromUtf8(err_text)), i18n("FITS Save"));
            return;
        }

        //statusBar()->changeItem(i18n("File saved."), 3);

        emit newStatus(i18n("File saved."), FITS_MESSAGE);
        modifyFITSState();
    } else
    {
        QString message = i18n( "Invalid URL: %1", currentURL.url() );
        KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
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
    emit newStatus(QString("%1x%2").arg(image->getWidth()).arg(image->getHeight()), FITS_RESOLUTION);
}


