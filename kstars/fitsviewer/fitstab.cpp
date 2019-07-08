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

#include "fitstab.h"

#include "fitsdata.h"
#include "fitshistogram.h"
#include "fitsview.h"
#include "fitsviewer.h"
#include "kstars.h"
#include "Options.h"
#include "ui_fitsheaderdialog.h"
#include "ui_statform.h"

#include <QtConcurrent>
#include <KMessageBox>

FITSTab::FITSTab(FITSViewer *parent) : QWidget(parent)
{
    viewer    = parent;
    undoStack = new QUndoStack(this);
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));

    statWidget = new QDialog(this);
    fitsHeaderDialog = new QDialog(this);
    histogram = new FITSHistogram(this);
}

FITSTab::~FITSTab()
{
    // Make sure it's done
    //histogramFuture.waitForFinished();
    //disconnect();
}

void FITSTab::saveUnsaved()
{
    if (undoStack->isClean() || view->getMode() != FITS_NORMAL)
        return;

    QString caption = i18n("Save Changes to FITS?");
    QString message = i18n("The current FITS file has unsaved changes.  Would you like to save before closing it?");
    int ans =
        KMessageBox::warningYesNoCancel(nullptr, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard());
    if (ans == KMessageBox::Yes)
        saveFile();
    if (ans == KMessageBox::No)
    {
        undoStack->clear();
        modifyFITSState();
    }
}

void FITSTab::closeEvent(QCloseEvent *ev)
{
    saveUnsaved();

    if (undoStack->isClean())
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

void FITSTab::selectRecentFITS(int i)
{
    loadFITS(QUrl::fromLocalFile(recentImages->item(i)->text()));
}

void FITSTab::clearRecentFITS()
{
    disconnect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
    recentImages->clear();
    connect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
}

void FITSTab::loadFITS(const QUrl &imageURL, FITSMode mode, FITSScale filter, bool silent)
{
    if (view.get() == nullptr)
    {
        view.reset(new FITSView(this, mode, filter));
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *vlayout = new QVBoxLayout();

        fitsSplitter = new QSplitter(Qt::Horizontal, this);
        fitsTools = new QToolBox();

        stat.setupUi(statWidget);
        fitsTools->addItem(statWidget, i18n("Statistics"));

        fitsTools->addItem(histogram, i18n("Histogram"));

        header.setupUi(fitsHeaderDialog);
        fitsTools->addItem(fitsHeaderDialog, i18n("FITS Header"));

        QVBoxLayout *recentPanelLayout = new QVBoxLayout();
        QWidget *recentPanel = new QWidget(fitsSplitter);
        recentPanel->setLayout(recentPanelLayout);
        fitsTools->addItem(recentPanel, i18n("Recent Images"));
        recentImages = new QListWidget(recentPanel);
        recentPanelLayout->addWidget(recentImages);
        QPushButton *clearRecent = new QPushButton(i18n("Clear"));
        recentPanelLayout->addWidget(clearRecent);
        connect(clearRecent, &QPushButton::pressed, this, &FITSTab::clearRecentFITS);
        connect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);

        QScrollArea *scrollFitsPanel = new QScrollArea(fitsSplitter);
        scrollFitsPanel->setWidgetResizable(true);
        scrollFitsPanel->setWidget(fitsTools);

        fitsSplitter->addWidget(scrollFitsPanel);
        fitsSplitter->addWidget(view.get());


        //This code allows the fitsTools to start in a closed state
        fitsSplitter->setSizes(QList<int>() << 0 << view->width() );

        vlayout->addWidget(fitsSplitter);

        connect(fitsSplitter, &QSplitter::splitterMoved, histogram, &FITSHistogram::resizePlot);


        setLayout(vlayout);
        connect(view.get(), &FITSView::newStatus, this, &FITSTab::newStatus);
        connect(view.get(), &FITSView::debayerToggled, this, &FITSTab::debayerToggled);

        // On Failure to load
        connect(view.get(), &FITSView::failed, this, &FITSTab::failed);

        // On Success loading image
        connect(view.get(), &FITSView::loaded, [&]()
        {
            // If it was already running make sure it's done
            //histogramFuture.waitForFinished();
            FITSData *image_data = view->getImageData();
            histogram->reset();
            image_data->setHistogram(histogram);

            // Only construct histogram if it is actually visible
            // Otherwise wait until histogram is needed before creating it.
            if (fitsSplitter->sizes().at(0) != 0)
            {
                histogram->constructHistogram();
                evaluateStats();
            }

            //            if (histogram == nullptr)
            //            {
            //                histogram = new FITSHistogram(this);
            //                image_data->setHistogram(histogram);
            //            }

            //histogramFuture = QtConcurrent::run([&]() {histogram->constructHistogram(); evaluateStats();});

            //if(histogram->isVisible())
            //    histogramFuture.waitForFinished();

            //            if (filter != FITS_NONE)
            //            {
            //                image_data->applyFilter(filter);
            //                view->rescale(ZOOM_KEEP_LEVEL);
            //            }

            if (viewer->isStarsMarked())
                view->toggleStars(true);


            loadFITSHeader();

            if(recentImages->findItems(currentURL.toLocalFile(), Qt::MatchExactly).count() == 0) //Don't add it to the list if it is already there
            {
                if(!image_data->isTempFile()) //Don't add it to the list if it is a preview
                {
                    disconnect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
                    recentImages->addItem(currentURL.toLocalFile());
                    recentImages->setCurrentRow(recentImages->count() - 1);
                    connect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
                }
            }

            view->updateFrame();

            emit loaded();
        });
    }

    currentURL = imageURL;

    view->setFilter(filter);

    view->loadFITS(imageURL.toLocalFile(), silent);
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
    QApplication::clipboard()->setImage(view->getDisplayImage());
}

void FITSTab::histoFITS()
{
    if (!histogram->isConstructed())
    {
        histogram->constructHistogram();
        evaluateStats();
    }

    fitsTools->setCurrentIndex(1);
    if(view->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << view->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
}

void FITSTab::evaluateStats()
{
    FITSData *image_data = view->getImageData();

    stat.widthOUT->setText(QString::number(image_data->width()));
    stat.heightOUT->setText(QString::number(image_data->height()));
    stat.bitpixOUT->setText(QString::number(image_data->bpp()));
    stat.maxOUT->setText(QString::number(image_data->getMax(), 'f', 3));
    stat.minOUT->setText(QString::number(image_data->getMin(), 'f', 3));
    stat.meanOUT->setText(QString::number(image_data->getMean(), 'f', 3));
    stat.stddevOUT->setText(QString::number(image_data->getStdDev(), 'f', 3));
    stat.HFROUT->setText(QString::number(image_data->getHFR(), 'f', 3));
    stat.medianOUT->setText(QString::number(image_data->getMedian(), 'f', 3));
    stat.SNROUT->setText(QString::number(image_data->getSNR(), 'f', 3));
}

void FITSTab::statFITS()
{
    fitsTools->setCurrentIndex(0);
    if(view->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << view->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
}

void FITSTab::loadFITSHeader()
{
    FITSData *image_data = view->getImageData();

    int nkeys = image_data->getRecords().size();
    int counter = 0;
    header.tableWidget->setRowCount(nkeys);
    for (FITSData::Record *oneRecord : image_data->getRecords())
    {
        QTableWidgetItem *tempItem = new QTableWidgetItem(oneRecord->key);
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(counter, 0, tempItem);
        tempItem = new QTableWidgetItem(oneRecord->value.toString());
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(counter, 1, tempItem);
        tempItem = new QTableWidgetItem(oneRecord->comment);
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(counter, 2, tempItem);
        counter++;
    }

    header.tableWidget->setColumnWidth(0, 100);
    header.tableWidget->setColumnWidth(1, 100);
    header.tableWidget->setColumnWidth(2, 250);
}

void FITSTab::headerFITS()
{
    fitsTools->setCurrentIndex(2);
    if(view->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << view->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
}

bool FITSTab::saveFile()
{
    QUrl backupCurrent = currentURL;
    QUrl currentDir(Options::fitsDir());
    currentDir.setScheme("file");

    if (currentURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || currentURL.toLocalFile().contains("/Temp"))
        currentURL.clear();

    // If no changes made, return.
    if (mDirty == false && !currentURL.isEmpty())
        return false;

    if (currentURL.isEmpty())
    {
        currentURL =
            QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save FITS"), currentDir, "FITS (*.fits *.fits.gz *.fit)");
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

    if (currentURL.isValid())
    {
        int err_status = 0;

        if ((err_status = saveFITS('!' + currentURL.toLocalFile())) != 0)
        {
            // -1000 = user canceled
            if (err_status == -1000)
                return false;

            char err_text[FLEN_STATUS];
            fits_get_errstatus(err_status, err_text);
            // Use KMessageBox or something here
            KMessageBox::error(nullptr, i18n("FITS file save error: %1", QString::fromUtf8(err_text)), i18n("FITS Save"));
            return false;
        }

        //statusBar()->changeItem(i18n("File saved."), 3);

        emit newStatus(i18n("File saved to %1", currentURL.url()), FITS_MESSAGE);
        modifyFITSState();
        return true;
    }
    else
    {
        QString message = i18n("Invalid URL: %1", currentURL.url());
        KMessageBox::sorry(nullptr, message, i18n("Invalid URL"));
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
    QPoint oldCenter = view->getImagePoint(view->viewport()->rect().center());
    view->ZoomIn();
    view->cleanUpZoom(oldCenter);
}

void FITSTab::ZoomOut()
{
    QPoint oldCenter = view->getImagePoint(view->viewport()->rect().center());
    view->ZoomOut();
    view->cleanUpZoom(oldCenter);
}

void FITSTab::ZoomDefault()
{
    QPoint oldCenter = view->getImagePoint(view->viewport()->rect().center());
    view->ZoomDefault();
    view->cleanUpZoom(oldCenter);
}

void FITSTab::tabPositionUpdated()
{
    undoStack->setActive(true);
    emit newStatus(QString("%1%").arg(view->getCurrentZoom()), FITS_ZOOM);
    emit newStatus(QString("%1x%2").arg(view->getImageData()->width()).arg(view->getImageData()->height()),
                   FITS_RESOLUTION);
}
