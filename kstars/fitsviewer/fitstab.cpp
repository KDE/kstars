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

#include <QUndoStack>
#include <KLocalizedString>
#include <KMessageBox>
#include <QFileDialog>

#include "Options.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitshistogram.h"
#include "fitsviewer.h"
#include "kstars.h"

#include "ui_statform.h"
#include "ui_fitsheaderdialog.h"

FITSTab::FITSTab(FITSViewer *parent) : QWidget()
{
    view      = NULL;
    histogram  = NULL;
    viewer     = parent;

    mDirty     = false;
    undoStack = new QUndoStack(this);
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));
}

FITSTab::~FITSTab()
{
    delete(view);
    disconnect(0,0,0);
}

void FITSTab::saveUnsaved()
{

    if( undoStack->isClean() || view->getMode() != FITS_NORMAL)
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
QString FITSTab::getPreviewText() const
{
    return previewText;
}

void FITSTab::setPreviewText(const QString &value)
{
    previewText = value;
}


bool FITSTab::loadFITS(const QUrl *imageURL, FITSMode mode, FITSScale filter, bool silent)
{
    if (view == NULL)
    {
        view = new FITSView(this, mode, filter);
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *vlayout = new QVBoxLayout();

        vlayout->addWidget(view);

        setLayout(vlayout);
        connect(view, SIGNAL(newStatus(QString,FITSBar)), this, SIGNAL(newStatus(QString,FITSBar)));
        connect(view, SIGNAL(debayerToggled(bool)), this, SIGNAL(debayerToggled(bool)));
    }

    currentURL = *imageURL;

    view->setFilter(filter);

    bool imageLoad = view->loadFITS(imageURL->toLocalFile(), silent);

    if (imageLoad)
    {
        if (histogram == NULL)
            histogram = new FITSHistogram(this);
        else
            histogram->constructHistogram();

        FITSData *image_data = view->getImageData();

        image_data->setHistogram(histogram);
        image_data->applyFilter(filter);

        if (filter != FITS_NONE)
            view->rescale(ZOOM_KEEP_LEVEL);

        if (viewer->isStarsMarked())
            view->toggleStars(true);

        view->updateFrame();
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
    return view->saveFITS(filename);
}

void FITSTab::copyFITS()
{
    QApplication::clipboard()->setImage( *(view->getDisplayImage()) );
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

    FITSData *image_data = view->getImageData();

    stat.widthOUT->setText(QString::number(image_data->getWidth()));
    stat.heightOUT->setText(QString::number(image_data->getHeight()));
    stat.bitpixOUT->setText(QString::number(image_data->getBPP()));
    stat.maxOUT->setText(QString::number(image_data->getMax(), 'f', 3));
    stat.minOUT->setText(QString::number(image_data->getMin() , 'f', 3));
    stat.meanOUT->setText(QString::number(image_data->getMean(), 'f' , 3));
    stat.stddevOUT->setText(QString::number(image_data->getStdDev(), 'f', 3));
    stat.HFROUT->setText(QString::number(image_data->getHFR(), 'f', 3));
    stat.medianOUT->setText(QString::number(image_data->getMedian(), 'f', 3));
    stat.SNROUT->setText(QString::number(image_data->getSNR(), 'f', 3));

    statDialog.exec();
}

void FITSTab::headerFITS()
{
    QString recordList;
    int nkeys;
    int err_status;
    char err_text[FLEN_STATUS];

    FITSData *image_data = view->getImageData();

    if ( (err_status = image_data->getFITSRecord(recordList, nkeys)) < 0)
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


bool FITSTab::saveFile()
{
    int err_status;
    char err_text[FLEN_STATUS];

    QUrl backupCurrent = currentURL;
    QUrl currentDir(Options::fitsDir());
    currentDir.setScheme("file");

    if (currentURL.toLocalFile().startsWith("/tmp/") || currentURL.toLocalFile().contains("/Temp"))
        currentURL.clear();

    // If no changes made, return.
    if( mDirty == false && !currentURL.isEmpty())
        return false;

    if (currentURL.isEmpty())
    {
        currentURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save FITS"), currentDir, "FITS (*.fits *.fit)");
        // if user presses cancel
        if (currentURL.isEmpty())
        {
            currentURL = backupCurrent;
            return false;
        }

        if (currentURL.toLocalFile().contains('.') == 0)
            currentURL.setPath(currentURL.toLocalFile() + ".fits");

        // Already display by dialog
        /*if (QFile::exists(currentURL.toLocalFile()))
        {
            int r = KMessageBox::warningContinueCancel(0,
                        i18n( "A file named \"%1\" already exists. "
                              "Overwrite it?", currentURL.fileName() ),
                        i18n( "Overwrite File?" ),
                        KGuiItem(i18n( "&Overwrite" )) );
            if(r==KMessageBox::Cancel)
                return false;

        }*/
    }

    if ( currentURL.isValid() )
    {
        if ( (err_status = saveFITS('!' + currentURL.toLocalFile())) != 0)
        {
            // -1000 = user canceled
            if (err_status == -1000)
                return false;

            fits_get_errstatus(err_status, err_text);
            // Use KMessageBox or something here
            KMessageBox::error(0, i18n("FITS file save error: %1",
                                       QString::fromUtf8(err_text)), i18n("FITS Save"));
            return false;
        }

        //statusBar()->changeItem(i18n("File saved."), 3);

        emit newStatus(i18n("File saved to %1", currentURL.url()), FITS_MESSAGE);
        modifyFITSState();
        return true;
    } else
    {
        QString message = i18n( "Invalid URL: %1", currentURL.url() );
        KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
        return false;
    }
}

bool FITSTab::saveFileAs()
{
    currentURL.clear();
    return saveFile();
}

void FITSTab::ZoomIn()
{
    QPoint oldCenter=view->getImagePoint(view->viewport()->rect().center());
    view->ZoomIn();
    view->cleanUpZoom(oldCenter);
}

void FITSTab::ZoomOut()
{
    QPoint oldCenter=view->getImagePoint(view->viewport()->rect().center());
    view->ZoomOut();
    view->cleanUpZoom(oldCenter);
}

void FITSTab::ZoomDefault()
{
    QPoint oldCenter=view->getImagePoint(view->viewport()->rect().center());
    view->ZoomDefault();
    view->cleanUpZoom(oldCenter);
}

void FITSTab::tabPositionUpdated()
{
    undoStack->setActive(true);
    emit newStatus(QString("%1%").arg(view->getCurrentZoom()), FITS_ZOOM);
    emit newStatus(QString("%1x%2").arg(view->getImageData()->getWidth()).arg(view->getImageData()->getHeight()), FITS_RESOLUTION);
}
