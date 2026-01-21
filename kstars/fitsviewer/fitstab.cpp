/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitstab.h"

#include "QtNetwork/qnetworkreply.h"
#include "fitsdata.h"
#include "fitshistogramcommand.h"
#include "fitsview.h"
#include "fitsviewer.h"
#include "fitsmemmonitor.h"
#include "ksnotification.h"
#include "kstars.h"
#include "Options.h"
#include "platesolve.h"
#include "ui_fitsheaderdialog.h"
#include "ui_statform.h"
#include "fitsstretchui.h"
#include "skymap.h"
#include <KMessageBox>
#include <QFileDialog>
#include <QClipboard>
#include <QIcon>

#include <fits_debug.h>
#include "fitscommon.h"
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>

#include <KLocalizedString>

constexpr int CAT_OBJ_SORT_ROLE = Qt::UserRole + 1;
const auto TEXT_START = ki18n("Start");
const auto TEXT_STOP = ki18n("Stop");
const auto TEXT_STOPPING = ki18n("Stopping...");

FITSTab::FITSTab(FITSViewer *parent) : QWidget(parent)
{
    viewer    = parent;
    undoStack = new QUndoStack(this);
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));

    m_PlateSolve = new PlateSolve(this);
    m_CatalogObjectWidget = new QDialog(this);
    m_LiveStackingWidget = new QDialog(this);
    statWidget = new QDialog(this);
    fitsHeaderDialog = new QDialog(this);
    m_HistogramEditor = new FITSHistogramEditor(this);
    connect(m_HistogramEditor, &FITSHistogramEditor::newHistogramCommand, this, [this](FITSHistogramCommand * command)
    {
        undoStack->push(command);
    });
    // Setup the default stack filter for file choosers
    const QStringList allPatterns = STACK_FITS_FILTER + STACK_XISF_FILTER;
    m_StackLastFilter = QString("All (%1)").arg(allPatterns.join(' '));
}

FITSTab::~FITSTab()
{
}

void FITSTab::saveUnsaved()
{
    if (undoStack->isClean() || m_View->getMode() != FITS_NORMAL)
        return;

    QString caption = i18n("Save Changes to FITS?");
    QString message = i18n("The current FITS file has unsaved changes.  Would you like to save before closing it?");

    int ans = KMessageBox::warningContinueCancel(nullptr, message, caption, KStandardGuiItem::save(),
              KStandardGuiItem::discard());
    if (ans == KMessageBox::Continue)
        saveFile();
    if (ans == KMessageBox::Cancel)
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
    loadFile(QUrl::fromLocalFile(recentImages->item(i)->text()));
}

void FITSTab::clearRecentFITS()
{
    disconnect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
    recentImages->clear();
    connect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
}

bool FITSTab::setupView(FITSMode mode, FITSScale filter)
{
    if (m_View.isNull())
    {
        m_View.reset(new FITSView(nullptr, mode, filter));
        m_View->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *vlayout = new QVBoxLayout();

        connect(m_View.get(), &FITSView::rectangleUpdated, this, [this](QRect roi)
        {
            displayStats(roi.isValid());
        });
        fitsSplitter = new QSplitter(Qt::Horizontal, this);
        fitsTools = new QToolBox();

        stat.setupUi(statWidget);

        for (int i = 0; i <= STAT_STDDEV; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                stat.statsTable->setItem(i, j, new QTableWidgetItem());
                stat.statsTable->item(i, j)->setTextAlignment(Qt::AlignHCenter);
            }

            // Set col span for items up to HFR
            if (i <= STAT_HFR)
                stat.statsTable->setSpan(i, 0, 1, 3);
        }

        connect(m_PlateSolve.data(), &PlateSolve::clicked, this, &FITSTab::extractImage);

        fitsTools->addItem(statWidget, i18n("Statistics"));
        fitsTools->addItem(m_PlateSolve, i18n("Plate Solving"));

        // Setup the Catalog Object page
        m_CatalogObjectUI.setupUi(m_CatalogObjectWidget);
        m_CatalogObjectItem = fitsTools->addItem(m_CatalogObjectWidget, i18n("Catalog Objects"));
        initCatalogObject();

        // Setup the Live Stacking page
        if (mode == FITS_LIVESTACKING)
        {
            m_LiveStackingUI.setupUi(m_LiveStackingWidget);
            m_LiveStackingUI.GeneralMinimizeWidget->setupUI(Options::fitsLSHideGeneral(), &Options::setFitsLSHideGeneral);
            m_LiveStackingUI.CalibrationMinimizeWidget->setupUI(Options::fitsLSHideCalibration(), &Options::setFitsLSHideCalibration);
            m_LiveStackingUI.AlignmentMinimizeWidget->setupUI(Options::fitsLSHideAlignment(), &Options::setFitsLSHideAlignment);
            m_LiveStackingUI.StackingMinimizeWidget->setupUI(Options::fitsLSHideStacking(), &Options::setFitsLSHideStacking);
            m_LiveStackingUI.PostProcMinimizeWidget->setupUI(Options::fitsLSHidePostProc(), &Options::setFitsLSHidePostProc);
            m_LiveStackingItem = fitsTools->addItem(m_LiveStackingWidget, i18n("Live Stacking"));
            initLiveStacking();
        }

        fitsTools->addItem(m_HistogramEditor, i18n("Histogram"));

        header.setupUi(fitsHeaderDialog);
        fitsTools->addItem(fitsHeaderDialog, i18n("FITS Header"));
        connect(m_View.get(), &FITSView::headerChanged, this, &FITSTab::loadFITSHeader);

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
        fitsSplitter->addWidget(m_View.get());

        //This code allows the fitsTools to start in a closed state
        fitsSplitter->setSizes(QList<int>() << 0 << m_View->width() );

        vlayout->addWidget(fitsSplitter);

        stretchUI.reset(new FITSStretchUI(m_View, nullptr));
        vlayout->addWidget(stretchUI.get());

        connect(fitsSplitter, &QSplitter::splitterMoved, m_HistogramEditor, &FITSHistogramEditor::resizePlot);

        setLayout(vlayout);
        connect(m_View.get(), &FITSView::newStatus, this, &FITSTab::newStatus);
        connect(m_View.get(), &FITSView::debayerToggled, this, &FITSTab::debayerToggled);
        connect(m_View.get(), &FITSView::updated, this, &FITSTab::updated);

        // On Failure to load
        connect(m_View.get(), &FITSView::failed, this, &FITSTab::failed);

        return true;
    }

    // returns false if no setup needed.
    return false;
}

void FITSTab::loadFile(const QUrl &imageURL, FITSMode mode, FITSScale filter)
{
    // check if the address points to an appropriate address
    if (imageURL.isEmpty() || !imageURL.isValid() || !QFileInfo::exists(imageURL.toLocalFile()))
    {
        emit failed(i18nc("File not found: %1", imageURL.toString().toLatin1()));
        return;
    }

    if (setupView(mode, filter))
    {

        // On Success loading image
        connect(m_View.get(), &FITSView::loaded, this, [&]()
        {
            processData();
            emit loaded();
        });

        connect(m_View.get(), &FITSView::updated, this, &FITSTab::updated);
    }
    else
        // update tab text
        modifyFITSState(true, imageURL);

    currentURL = imageURL;

    m_View->setFilter(filter);

    m_View->loadFile(imageURL.toLocalFile());
}

// Initialise the stack - signalled once from FitsViewer
void FITSTab::initStack(const QString &dir, FITSMode mode, FITSScale filter)
{
    // check if the address points to an appropriate address
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
    {
        emit failed(i18nc("Invalid directory: %1", dir.toLatin1()));
        return;
    }

    if (setupView(mode, filter))
    {
        // On Success loading image
        connect(m_View.get(), &FITSView::loaded, this, [&]()
        {
            processData();
            emit loaded();
        });

        connect(m_View.get(), &FITSView::updated, this, &FITSTab::updated);
    }
    else
        modifyFITSState(true, QUrl(dir));

    m_View->setFilter(filter);

    m_CurrentStackDir = dir;
    m_liveStackDir.push_back(dir);
    m_LiveStackingUI.Stack->setText(dir);

    // Popup the Live Stacking pane
    fitsTools->setCurrentIndex(m_LiveStackingItem);
    fitsTools->show();
    if(m_View->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);

    m_View->initStack();
}

bool FITSTab::shouldComputeHFR() const
{
    if (viewer->isStarsMarked())
        return true;
    if (!Options::autoHFR())
        return false;
    return ((!m_View.isNull()) && (m_View->getMode() == FITS_NORMAL));
}

void FITSTab::processData()
{
    const QSharedPointer<FITSData> &imageData = m_View->imageData();

    m_HistogramEditor->setImageData(imageData);

    // Only construct histogram if it is actually visible
    // Otherwise wait until histogram is needed before creating it.
    //    if (fitsSplitter->sizes().at(0) != 0 && !imageData->isHistogramConstructed() &&
    //            !Options::nonLinearHistogram())
    //    {
    //        imageData->constructHistogram();
    //    }

    if (shouldComputeHFR())
    {
        m_View->searchStars();
        qCDebug(KSTARS_FITS) << "FITS HFR:" << imageData->getHFR();
    }

    displayStats();

    loadFITSHeader();

    // Don't add it to the list if it is already there
    if (recentImages->findItems(currentURL.toLocalFile(), Qt::MatchExactly).count() == 0)
    {
        //Don't add it to the list if it is a preview
        if(!imageData->filename().startsWith(QDir::tempPath()))
        {
            disconnect(recentImages, &QListWidget::currentRowChanged, this,
                       &FITSTab::selectRecentFITS);
            recentImages->addItem(imageData->filename());
            recentImages->setCurrentRow(recentImages->count() - 1);
            connect(recentImages, &QListWidget::currentRowChanged,  this,
                    &FITSTab::selectRecentFITS);
        }
    }

    //     This could both compute the HFRs and setup the graphics, however,
    //     if shouldComputeHFR() above is true, then that will compute the HFRs
    //     and this would notice that and just setup graphics. They are separated
    //     for the case where the graphics is not desired.
    if (viewer->isStarsMarked())
    {
        m_View->toggleStars(true);
        m_View->updateFrame();
    }

    //    if (Options::nonLinearHistogram())
    //        m_HistogramEditor->createNonLinearHistogram();

    stretchUI->generateHistogram();
}

bool FITSTab::loadData(const QSharedPointer<FITSData> &data, FITSMode mode, FITSScale filter)
{
    setupView(mode, filter);

    // Empty URL
    currentURL = QUrl();

    if (viewer->isStarsMarked())
    {
        m_View->toggleStars(true);
        //view->updateFrame();
    }

    m_View->setFilter(filter);

    if (!m_View->loadData(data))
    {
        // On Failure to load
        // connect(view.get(), &FITSView::failed, this, &FITSTab::failed);
        return false;
    }

    processData();
    return true;
}

void FITSTab::modifyFITSState(bool clean, const QUrl &imageURL)
{
    if (clean)
    {
        if (undoStack->isClean() == false)
            undoStack->setClean();

        mDirty = false;
    }
    else
        mDirty = true;

    emit changeStatus(clean, imageURL);
}

bool FITSTab::saveImage(const QString &filename)
{
    return m_View->saveImage(filename);
}

void FITSTab::copyFITS()
{
    QApplication::clipboard()->setImage(m_View->getDisplayImage());
}

void FITSTab::histoFITS()
{
    fitsTools->setCurrentIndex(1);
    if(m_View->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
}

void FITSTab::displayStats(bool roi)
{
    const QSharedPointer<FITSData> &imageData = m_View->imageData();

    stat.statsTable->item(STAT_WIDTH, 0)->setText(QString::number(imageData->width(roi)));
    stat.statsTable->item(STAT_HEIGHT, 0)->setText(QString::number(imageData->height(roi)));
    stat.statsTable->item(STAT_BITPIX, 0)->setText(QString::number(imageData->bpp()));

    if (!roi)
        stat.statsTable->item(STAT_HFR, 0)->setText(QString::number(imageData->getHFR(), 'f', 3));
    else
        stat.statsTable->item(STAT_HFR, 0)->setText("---");

    if (imageData->channels() == 1)
    {
        for (int i = STAT_MIN; i <= STAT_STDDEV; i++)
        {
            if (stat.statsTable->columnSpan(i, 0) != 3)
                stat.statsTable->setSpan(i, 0, 1, 3);
        }

        stat.statsTable->horizontalHeaderItem(0)->setText(i18n("Value"));
        stat.statsTable->hideColumn(1);
        stat.statsTable->hideColumn(2);
    }
    else
    {
        for (int i = STAT_MIN; i <= STAT_STDDEV; i++)
        {
            if (stat.statsTable->columnSpan(i, 0) != 1)
                stat.statsTable->setSpan(i, 0, 1, 1);
        }

        stat.statsTable->horizontalHeaderItem(0)->setText(i18nc("Red", "R"));
        stat.statsTable->showColumn(1);
        stat.statsTable->showColumn(2);
    }

    if (!Options::nonLinearHistogram() && !imageData->isHistogramConstructed())
        imageData->constructHistogram();

    for (int i = 0; i < imageData->channels(); i++)
    {
        stat.statsTable->item(STAT_MIN, i)->setText(QString::number(imageData->getMin(i, roi), 'f', 3));
        stat.statsTable->item(STAT_MAX, i)->setText(QString::number(imageData->getMax(i, roi), 'f', 3));
        stat.statsTable->item(STAT_MEAN, i)->setText(QString::number(imageData->getMean(i, roi), 'f', 3));
        stat.statsTable->item(STAT_MEDIAN, i)->setText(QString::number(imageData->getMedian(i, roi), 'f', 3));
        stat.statsTable->item(STAT_STDDEV, i)->setText(QString::number(imageData->getStdDev(i, roi), 'f', 3));
    }
}

void FITSTab::statFITS()
{
    fitsTools->setCurrentIndex(0);
    if(m_View->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
    displayStats();
}

void FITSTab::loadFITSHeader()
{
    const QSharedPointer<FITSData> &imageData = m_View->imageData();
    if (!imageData)
        return;

    int nkeys = imageData->getRecords().size();
    int counter = 0;
    header.tableWidget->setRowCount(nkeys);
    for (const auto &oneRecord : imageData->getRecords())
    {
        QTableWidgetItem *tempItem = new QTableWidgetItem(oneRecord.key);
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(counter, 0, tempItem);
        tempItem = new QTableWidgetItem(oneRecord.value.toString());
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(counter, 1, tempItem);
        tempItem = new QTableWidgetItem(oneRecord.comment);
        tempItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        header.tableWidget->setItem(counter, 2, tempItem);
        counter++;
    }

    header.tableWidget->setColumnWidth(0, 100);
    header.tableWidget->setColumnWidth(1, 100);
    header.tableWidget->setColumnWidth(2, 250);
}

void FITSTab::loadCatalogObjects()
{
    // Check pointers are OK
    if (!m_View)
        return;
    const QSharedPointer<FITSData> &imageData = m_View->imageData();
    if (!imageData)
        return;
    QList<CatObject> catObjects = imageData->getCatObjects();

    // Disable sorting whilst building the table
    m_CatalogObjectUI.tableView->setSortingEnabled(false);
    // Remove all rows
    m_CatObjModel.removeRows(0, m_CatObjModel.rowCount());

    int counter = 0, total = 0, highlightRow = -1;
    QPixmap cdsPortalPixmap = QPixmap(":/icons/cdsportal.svg").scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap simbadPixmap = QPixmap(":/icons/simbad.svg").scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap nedPixmap = QPixmap(":/icons/NED.png").scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    for (const CatObject &catObject : catObjects)
    {
        total++;
        if (!catObject.show)
            continue;
        m_CatObjModel.setRowCount(counter + 1);

        // Num
        QStandardItem* tempItem = new QStandardItem(QString::number(catObject.num));
        tempItem->setData(catObject.num, CAT_OBJ_SORT_ROLE);
        tempItem->setTextAlignment(Qt::AlignRight);
        m_CatObjModel.setItem(counter, CAT_NUM, tempItem);

        // CDS Portal - no sorting
        tempItem = new QStandardItem();
        tempItem->setData(cdsPortalPixmap, Qt::DecorationRole);
        m_CatObjModel.setItem(counter, CAT_CDSPORTAL, tempItem);

        // Simbad - no sorting
        tempItem = new QStandardItem();
        tempItem->setData(simbadPixmap, Qt::DecorationRole);
        m_CatObjModel.setItem(counter, CAT_SIMBAD, tempItem);

        // NED - no sorting
        tempItem = new QStandardItem();
        tempItem->setData(nedPixmap, Qt::DecorationRole);
        m_CatObjModel.setItem(counter, CAT_NED, tempItem);

        // Object
        tempItem = new QStandardItem(catObject.name);
        tempItem->setData(catObject.name, CAT_OBJ_SORT_ROLE);
        m_CatObjModel.setItem(counter, CAT_OBJECT, tempItem);

        // Type code
        tempItem = new QStandardItem(catObject.typeCode);
        tempItem->setData(catObject.typeCode, CAT_OBJ_SORT_ROLE);
        m_CatObjModel.setItem(counter, CAT_TYPECODE, tempItem);

        // Type label
        tempItem = new QStandardItem(catObject.typeLabel);
        tempItem->setData(catObject.typeLabel, CAT_OBJ_SORT_ROLE);
        m_CatObjModel.setItem(counter, CAT_TYPELABEL, tempItem);

        // Coordinates
        QString coordStr = QString("%1 %2").arg(catObject.r.toHMSString(false, true))
                           .arg(catObject.d.toDMSString(true, false, true));
        tempItem = new QStandardItem(coordStr);
        tempItem->setData(coordStr, CAT_OBJ_SORT_ROLE);
        m_CatObjModel.setItem(counter, CAT_COORDS, tempItem);

        // Distance
        tempItem = new QStandardItem(QString::number(catObject.dist));
        tempItem->setData(catObject.dist, CAT_OBJ_SORT_ROLE);
        tempItem->setTextAlignment(Qt::AlignRight);
        m_CatObjModel.setItem(counter, CAT_DISTANCE, tempItem);

        // Magnitude
        QString magStr = (catObject.magnitude <= 0.0) ? "" : QString("%1").arg(catObject.magnitude, 0, 'f', 1);
        tempItem = new QStandardItem(magStr);
        tempItem->setData(catObject.magnitude, CAT_OBJ_SORT_ROLE);
        tempItem->setTextAlignment(Qt::AlignRight);
        m_CatObjModel.setItem(counter, CAT_MAGNITUDE, tempItem);

        // Size
        QString sizeStr = (catObject.size <= 0.0) ? "" : QString("%1").arg(catObject.size, 0, 'f', 0);
        tempItem = new QStandardItem(sizeStr);
        tempItem->setData(catObject.size, CAT_OBJ_SORT_ROLE);
        tempItem->setTextAlignment(Qt::AlignRight);
        m_CatObjModel.setItem(counter, CAT_SIZE, tempItem);

        if (catObject.highlight)
            highlightRow = counter;

        counter++;
    }

    // Resize the columns to the data
    m_CatalogObjectUI.tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    if (highlightRow >= 0)
        catHighlightRow(highlightRow);

    // Enable sorting
    m_CatObjModel.setSortRole(CAT_OBJ_SORT_ROLE);
    m_CatalogObjectUI.tableView->setSortingEnabled(true);

    // Update the status widget unless the query is still in progress
    if (!imageData->getCatQueryInProgress())
        m_CatalogObjectUI.status->setText(i18n("%1 / %2 Simbad objects loaded", counter, total));
}

void FITSTab::queriedCatalogObjects()
{
    // Show the Catalog Objects item (unless already shown)
    if (fitsTools->currentIndex() != m_CatalogObjectItem)
    {
        fitsTools->setCurrentIndex(m_CatalogObjectItem);
        if(m_View->width() > 200)
            fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
        else
            fitsSplitter->setSizes(QList<int>() << 50 << 50);
    }

    m_CatalogObjectUI.status->setText(i18n("Simbad query in progress..."));
}

void FITSTab::catQueryFailed(const QString text)
{
    m_CatalogObjectUI.status->setText(text);
}

void FITSTab::catReset()
{
    m_CatalogObjectUI.status->setText("");
    // Remove all rows from the table
    m_CatObjModel.removeRows(0, m_CatObjModel.rowCount());
}

void FITSTab::catHighlightRow(const int row)
{
    m_CatalogObjectUI.tableView->selectRow(row);
    QModelIndex index = m_CatalogObjectUI.tableView->indexAt(QPoint(row, CAT_NUM));
    if (index.isValid())
        m_CatalogObjectUI.tableView->scrollTo(index, QAbstractItemView::EnsureVisible);
}

void FITSTab::catHighlightChanged(const int highlight)
{
    if (!m_View)
        return;
    const QSharedPointer<FITSData> &imageData = m_View->imageData();
    if (!imageData)
        return;
    QList<CatObject> catObjects = imageData->getCatObjects();

    if (highlight < 0 || highlight >= catObjects.size())
        return;

    int num = catObjects[highlight].num;
    for (int i = 0; i < m_CatObjModel.rowCount(); i++)
    {
        bool ok;
        QStandardItem *itm = m_CatObjModel.item(i, CAT_NUM);
        if (itm->text().toInt(&ok) == num)
        {
            int itmRow = itm->row();
            QModelIndex currentIndex = m_CatalogObjectUI.tableView->currentIndex();
            if (currentIndex.isValid())
            {
                int currentRow = m_CatObjModel.itemFromIndex(currentIndex)->row();
                if (currentRow == itmRow)
                    // Row to highlight is already highlighted - so nothing to do
                    break;
            }
            // Set the new highlight to the new row
            catHighlightRow(itmRow);
        }
    }
}

void FITSTab::catRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    if (!m_View)
        return;
    const QSharedPointer<FITSData> &imageData = m_View->imageData();
    if (!imageData)
        return;

    // Get the "Num" of the selected (current) row
    int selNum = -1, deselNum = -1;
    bool ok;
    QStandardItem * selItem = m_CatObjModel.itemFromIndex(current);
    if (selItem)
        selNum = m_CatObjModel.item(selItem->row(), CAT_NUM)->text().toInt(&ok);

    // Get the "Num" of the deselected (previous) row
    QStandardItem * deselItem = m_CatObjModel.itemFromIndex(previous);
    if (deselItem)
        deselNum = m_CatObjModel.item(deselItem->row(), CAT_NUM)->text().toInt(&ok);

    if (selNum >= 0)
    {
        imageData->highlightCatObject(selNum - 1, deselNum - 1);
        m_View->updateFrame();
    }
}

void FITSTab::catCellDoubleClicked(const QModelIndex &index)
{
    QStandardItem * item = m_CatObjModel.itemFromIndex(index);
    int row = item->row();
    int col = item->column();

    QString catObjectName = m_CatObjModel.item(row, CAT_OBJECT)->text();

    if (col == CAT_CDSPORTAL)
        launchCDS(catObjectName);
    else if (col == CAT_SIMBAD)
        launchSimbad(catObjectName);
    else if (col == CAT_NED)
        launchNED(catObjectName);
    else if (col == CAT_TYPECODE || col == CAT_TYPELABEL)
        launchCatTypeFilterDialog();
}

void FITSTab::launchCatTypeFilterDialog()
{
    m_CatObjTypeFilterDialog->show();
    m_CatObjTypeFilterDialog->raise();
}

void FITSTab::showCatObjNames(bool enabled)
{
    if (!m_View)
        return;
    const QSharedPointer<FITSData> &imageData = m_View->imageData();
    if (!imageData)
        return;

    Options::setFitsCatObjShowNames(enabled);
    m_View->updateFrame();
}

void FITSTab::launchSimbad(QString name)
{
    QUrl url = QUrl("https://simbad.u-strasbg.fr/simbad/sim-id");
    QString ident = QString("Ident=%1").arg(name);
    ident.replace("+", "%2B");
    ident.remove(QRegularExpression("[\\[\\]]+"));
    url.setQuery(ident);

    if (!QDesktopServices::openUrl(url))
        qCDebug(KSTARS_FITS) << "Unable to open Simbad url:" << url;
}

void FITSTab::launchCDS(QString name)
{
    QUrl url = QUrl("https://cdsportal.u-strasbg.fr/");
    QString ident = QString("target=%1").arg(name);
    ident.replace("+", "%2B");
    ident.remove(QRegularExpression("[\\[\\]]+"));
    url.setQuery(ident);

    if (!QDesktopServices::openUrl(url))
        qCDebug(KSTARS_FITS) << "Unable to open CDS Portal url:" << url;
}

void FITSTab::launchNED(QString name)
{
    QUrl url = QUrl("https://ned.ipac.caltech.edu/cgi-bin/objsearch");
    QString query = QString("objname=%1").arg(name);
    query.replace("+", "%2B");
    query.remove(QRegularExpression("[\\[\\]]+"));
    url.setQuery(query);

    if (!QDesktopServices::openUrl(url))
        qCDebug(KSTARS_FITS) << "Unable to open NED url" << url;
}

void FITSTab::initCatalogObject()
{
    // Setup MVC
    m_CatalogObjectUI.tableView->setModel(&m_CatObjModel);

    // Set the column headers
    QStringList Headers { i18n("Num"),
                          i18n("CDS Portal"),
                          i18n("Simbad"),
                          i18n("NED"),
                          i18n("Object"),
                          i18n("Type Code"),
                          i18n("Type Label"),
                          i18n("Coordinates"),
                          i18n("Distance"),
                          i18n("Magnitude"),
                          i18n("Size") };
    m_CatObjModel.setColumnCount(CAT_MAX_COLS);
    m_CatObjModel.setHorizontalHeaderLabels(Headers);

    // Setup tooltips on column headers
    m_CatObjModel.setHeaderData(CAT_NUM, Qt::Horizontal, i18n("Item reference number"), Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_CDSPORTAL, Qt::Horizontal, i18n("Double click to launch CDS Portal browser"),
                                Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_SIMBAD, Qt::Horizontal, i18n("Double click to launch Simbad browser"), Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_NED, Qt::Horizontal, i18n("Double click to launch NED browser"), Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_OBJECT, Qt::Horizontal, i18n("Catalog Object"), Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_TYPECODE, Qt::Horizontal,
                                i18n("Object Type Code. Double click to launch Object Type Filter.\n\nSee https://simbad.cds.unistra.fr/guide/otypes.htx for more details"),
                                Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_TYPELABEL, Qt::Horizontal,
                                i18n("Object Type Label. Double click to launch Object Type Filter.\n\nSee https://simbad.cds.unistra.fr/guide/otypes.htx for more details"),
                                Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_COORDS, Qt::Horizontal, i18n("Object coordinates"), Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_DISTANCE, Qt::Horizontal, i18n("Object distance in arcmins from the search center"),
                                Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_MAGNITUDE, Qt::Horizontal, i18n("Object V magnitude"), Qt::ToolTipRole);
    m_CatObjModel.setHeaderData(CAT_SIZE, Qt::Horizontal, i18n("Object major coordinate size in arcsmins"), Qt::ToolTipRole);

    // Setup delegates for each column
    QStyledItemDelegate *delegate = new QStyledItemDelegate(m_CatalogObjectUI.tableView);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_NUM, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_CDSPORTAL, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_SIMBAD, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_NED, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_OBJECT, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_TYPECODE, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_TYPELABEL, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_COORDS, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_DISTANCE, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_MAGNITUDE, delegate);
    m_CatalogObjectUI.tableView->setItemDelegateForColumn(CAT_SIZE, delegate);

    m_CatalogObjectUI.tableView->setAutoScroll(true);

    connect(m_View.get(), &FITSView::catLoaded, this, &FITSTab::loadCatalogObjects);
    connect(m_View.get(), &FITSView::catQueried, this, &FITSTab::queriedCatalogObjects);
    connect(m_View.get(), &FITSView::catQueryFailed, this, &FITSTab::catQueryFailed);
    connect(m_View.get(), &FITSView::catReset, this, &FITSTab::catReset);
    connect(m_View.get(), &FITSView::catHighlightChanged, this, &FITSTab::catHighlightChanged);
    connect(m_CatalogObjectUI.tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            &FITSTab::catRowChanged);
    connect(m_CatalogObjectUI.tableView, &QAbstractItemView::doubleClicked, this, &FITSTab::catCellDoubleClicked);
    connect(m_CatalogObjectUI.filterPB, &QPushButton::clicked, this, &FITSTab::launchCatTypeFilterDialog);

    // Setup the Object Type filter popup
    setupCatObjTypeFilter();

    // Set the Show Names checkbox from Options
    m_CatalogObjectUI.kcfg_FitsCatObjShowNames->setChecked(Options::fitsCatObjShowNames());
    connect(m_CatalogObjectUI.kcfg_FitsCatObjShowNames, &QCheckBox::toggled, this, &FITSTab::showCatObjNames);
}

void FITSTab::setupCatObjTypeFilter()
{
    // Setup the dialog box
    m_CatObjTypeFilterDialog = new QDialog(this);
    m_CatObjTypeFilterUI.setupUi(m_CatObjTypeFilterDialog);
#ifdef Q_OS_MACOS
    m_CatObjTypeFilterDialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    // Setup the buttons
    QPushButton *applyButton = m_CatObjTypeFilterUI.buttonBox->button(QDialogButtonBox::Apply);
    connect(applyButton, &QPushButton::clicked, this, &FITSTab::applyTypeFilter);
    m_CheckAllButton = m_CatObjTypeFilterUI.buttonBox->addButton("Check All", QDialogButtonBox::ActionRole);
    connect(m_CheckAllButton, &QPushButton::clicked, this, &FITSTab::checkAllTypeFilter);
    m_UncheckAllButton = m_CatObjTypeFilterUI.buttonBox->addButton("Uncheck All", QDialogButtonBox::ActionRole);
    connect(m_UncheckAllButton, &QPushButton::clicked, this, &FITSTab::uncheckAllTypeFilter);

    // Setup the tree
    QTreeWidgetItem * treeItem[CAT_OBJ_MAX_DEPTH];
    for (int i = 0; i < MAX_CAT_OBJ_TYPES; i++)
    {
        const int depth = catObjTypes[i].depth;
        if (depth < 0 || depth >= MAX_CAT_OBJ_TYPES)
            continue;

        if (depth == 0)
            // Top level node
            treeItem[depth] = new QTreeWidgetItem(m_CatObjTypeFilterUI.tree);
        else
            // Child node
            treeItem[depth] = new QTreeWidgetItem(treeItem[depth - 1]);

        treeItem[depth]->setCheckState(CATTYPE_CODE, Qt::Unchecked);
        treeItem[depth]->setText(CATTYPE_CODE, catObjTypes[i].code);
        treeItem[depth]->setText(CATTYPE_CANDCODE, catObjTypes[i].candidateCode);
        treeItem[depth]->setText(CATTYPE_LABEL, catObjTypes[i].label);
        treeItem[depth]->setText(CATTYPE_DESCRIPTION, catObjTypes[i].description);
        treeItem[depth]->setText(CATTYPE_COMMENTS, catObjTypes[i].comments);
    }
    m_CatObjTypeFilterUI.tree->expandAll();
    for (int i = 0; i < CAT_OBJ_MAX_DEPTH; i++)
    {
        m_CatObjTypeFilterUI.tree->resizeColumnToContents(i);
    }

    connect(m_CatObjTypeFilterUI.tree, &QTreeWidget::itemChanged, this, &FITSTab::typeFilterItemChanged);
}

void FITSTab::initLiveStacking()
{
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
    // Setup the memory monitor widget
    m_LiveStackingUI.MemMonitor->setUpdateInterval(1000);
    QString label = i18n("RAM");
    m_LiveStackingUI.MemMonitor->setLabelFormat(label + ": %1 / %2");

    // Set the GUI values
    m_LiveStackingUI.SubsProcessed->setText(QString("%1 / %2 / %3").arg(0).arg(0).arg(0));
    m_LiveStackingUI.SubsSNR->setText(QString("%1 / %2 / %3").arg(0).arg(0).arg(0));
    m_LiveStackingUI.ImageSNR->setText(QString("%1").arg(0));
    m_LiveStackingUI.AlignMaster->setText("");
    initSettings();

    // Manage connections
    connect(m_LiveStackingUI.StackMultiCB1, &QToolButton::clicked, this, &FITSTab::toggleMultiC);
    connect(m_LiveStackingUI.StackMultiCB2, &QToolButton::clicked, this, &FITSTab::toggleMultiC);
    connect(m_LiveStackingUI.StackDirB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStack(m_LiveStackingUI.Stack, "Select Stack Directory");
    });
    connect(m_LiveStackingUI.RedDirB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStack(m_LiveStackingUI.Red, "Select Red Stack Directory");
    });
    connect(m_LiveStackingUI.GreenDirB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStack(m_LiveStackingUI.Green, "Select Green Stack Directory");
    });
    connect(m_LiveStackingUI.BlueDirB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStack(m_LiveStackingUI.Blue, "Select Blue Stack Directory");
    });
    connect(m_LiveStackingUI.LumDirB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStack(m_LiveStackingUI.Lum, "Select Lum Stack Directory");
    });

    connect(m_LiveStackingUI.Stack, &QLineEdit::textChanged, this, &FITSTab::stackDirChanged);
    connect(m_LiveStackingUI.Red, &QLineEdit::textChanged, this, &FITSTab::stackDirChanged);
    connect(m_LiveStackingUI.Green, &QLineEdit::textChanged, this, &FITSTab::stackDirChanged);
    connect(m_LiveStackingUI.Blue, &QLineEdit::textChanged, this, &FITSTab::stackDirChanged);

    connect(m_LiveStackingUI.HelpB, &QPushButton::clicked, this, &FITSTab::launchLiveStackingHelp);
    connect(m_LiveStackingUI.MonitorB, &QPushButton::clicked, this, [this]()
    {
        if (auto stackMonitor = m_View ? m_View->getStackMon() : nullptr)
        {
            stackMonitor->show();
            stackMonitor->raise();
            stackMonitor->activateWindow();
        }
    });

    connect(m_LiveStackingUI.StartB, &QPushButton::clicked, this, &FITSTab::liveStack);
    connect(m_LiveStackingUI.SaveB, &QPushButton::clicked, this, &FITSTab::saveSettings);
    connect(m_LiveStackingUI.ReprocessB, &QPushButton::clicked, this, &FITSTab::redoPostProcessing);

    connect(m_LiveStackingUI.CalcSNR, &QGroupBox::toggled, this, &FITSTab::calcSNRChanged);
    connect(m_LiveStackingUI.MasterDarkB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterDark, "Select Master Dark");
    });
    connect(m_LiveStackingUI.MasterDarkRedB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterDarkRed, "Select Master Dark (Red)");
    });
    connect(m_LiveStackingUI.MasterDarkGreenB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterDarkGreen, "Select Master Dark (Green)");
    });
    connect(m_LiveStackingUI.MasterDarkBlueB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterDarkBlue, "Select Master Dark (Blue)");
    });
    connect(m_LiveStackingUI.MasterDarkLumB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterDarkLum, "Select Master Dark (Lum)");
    });

    connect(m_LiveStackingUI.MasterFlatB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterFlat, "Select Master Flat");
    });
    connect(m_LiveStackingUI.MasterFlatRedB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterFlatRed, "Select Master Flat (Red)");
    });
    connect(m_LiveStackingUI.MasterFlatGreenB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterFlatGreen, "Select Master Flat (Green)");
    });
    connect(m_LiveStackingUI.MasterFlatBlueB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterFlatBlue, "Select Master Flat (Blue)");
    });
    connect(m_LiveStackingUI.MasterFlatLumB, &QPushButton::clicked, this,
            [this]()
    {
        selectLiveStackMaster(m_LiveStackingUI.MasterFlatLum, "Select Master Flat (Lum)");
    });

    connect(m_LiveStackingUI.AlignMasterB, &QPushButton::clicked, this, &FITSTab::selectLiveStackAlignSub);
    connect(m_LiveStackingUI.PostProcGroupBox, &QGroupBox::toggled, this, &FITSTab::redoPostProcessing);
    connect(m_LiveStackingUI.StackingMethod, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FITSTab::stackMethodChanged);

    // Other connections used by Live Stacking
    connect(m_View.get(), &FITSView::plateSolveSub, this, &FITSTab::plateSolveSub);
    connect(m_View.get(), &FITSView::stackInProgress, this, &FITSTab::stackInProgress);
    connect(m_View.get(), &FITSView::alignMasterChosen, this, &FITSTab::alignMasterChosen);
    connect(m_View.get(), &FITSView::stackUpdateStats, this, &FITSTab::stackUpdateStats);
    connect(m_View.get(), &FITSView::updateStackSNR, this, &FITSTab::updateStackSNR);
    connect(m_View.get(), &FITSView::resetStack, this, &FITSTab::resetStack);
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV
}

void FITSTab::initSettings()
{
    m_LiveStackingUI.CalcSNR->setChecked(Options::fitsLSCalcSNR());
    m_LiveStackingUI.MasterDark->setText(Options::fitsLSMasterDark());
    m_LiveStackingUI.MasterFlat->setText(Options::fitsLSMasterFlat());
    m_LiveStackingUI.HotPixels->setChecked(Options::fitsLSHotPixels());
    m_LiveStackingUI.ColdPixels->setChecked(Options::fitsLSColdPixels());
    m_LiveStackingUI.AlignMethod->setCurrentIndex(Options::fitsLSAlignMethod());
    m_LiveStackingUI.NumInMem->setValue(Options::fitsLSNumInMem());
    m_LiveStackingUI.LSDownscale->setCurrentIndex(Options::fitsLSDownscale());
    m_LiveStackingUI.Weighting->setCurrentIndex(Options::fitsLSWeighting());
    m_LiveStackingUI.StackingMethod->setCurrentIndex(Options::fitsLSStackingMethod());
    m_LiveStackingUI.LowSigma->setValue(Options::fitsLSLowSigma());
    m_LiveStackingUI.HighSigma->setValue(Options::fitsLSHighSigma());
    m_LiveStackingUI.WinsorCutoff->setValue(Options::fitsLSWinsorCutoff());
    m_LiveStackingUI.Iterations->setValue(Options::fitsLSIterations());
    m_LiveStackingUI.Kappa->setValue(Options::fitsLSKappa());
    m_LiveStackingUI.Alpha->setValue(Options::fitsLSAlpha());
    m_LiveStackingUI.Sigma->setValue(Options::fitsLSSigma());
    m_LiveStackingUI.PSFUpdate->setValue(Options::fitsPSFUpdate());
    m_LiveStackingUI.PostProcGroupBox->setChecked(Options::fitsLSPostProc());
    m_LiveStackingUI.DeconvAmt->setValue(Options::fitsLSDeconvAmt());
    m_LiveStackingUI.PSFSigma->setValue(Options::fitsLSPSFSigma());
    m_LiveStackingUI.DenoiseAmt->setValue(Options::fitsLSDenoiseAmt());
    m_LiveStackingUI.SharpenAmt->setValue(Options::fitsLSSharpenAmt());
    m_LiveStackingUI.SharpenKernal->setValue(Options::fitsLSSharpenKernal());
    m_LiveStackingUI.SharpenSigma->setValue(Options::fitsLSSharpenSigma());
    stackMethodChanged(m_LiveStackingUI.StackingMethod->currentIndex());
    toggleMultiC();
}

void FITSTab::saveSettings()
{
    Options::setFitsLSCalcSNR(m_LiveStackingUI.CalcSNR->isChecked());
    Options::setFitsLSMasterDark(m_LiveStackingUI.MasterDark->text());
    Options::setFitsLSMasterFlat(m_LiveStackingUI.MasterFlat->text());
    Options::setFitsLSHotPixels(m_LiveStackingUI.HotPixels->isChecked());
    Options::setFitsLSColdPixels(m_LiveStackingUI.ColdPixels->isChecked());
    Options::setFitsLSAlignMethod(m_LiveStackingUI.AlignMethod->currentIndex());
    Options::setFitsLSNumInMem(m_LiveStackingUI.NumInMem->value());
    Options::setFitsLSDownscale(m_LiveStackingUI.LSDownscale->currentIndex());
    Options::setFitsLSWeighting(m_LiveStackingUI.Weighting->currentIndex());
    Options::setFitsLSStackingMethod(m_LiveStackingUI.StackingMethod->currentIndex());
    Options::setFitsLSLowSigma(m_LiveStackingUI.LowSigma->value());
    Options::setFitsLSHighSigma(m_LiveStackingUI.HighSigma->value());
    Options::setFitsLSWinsorCutoff(m_LiveStackingUI.WinsorCutoff->value());
    Options::setFitsLSIterations(m_LiveStackingUI.Iterations->value());
    Options::setFitsLSKappa(m_LiveStackingUI.Kappa->value());
    Options::setFitsLSAlpha(m_LiveStackingUI.Alpha->value());
    Options::setFitsLSSigma(m_LiveStackingUI.Sigma->value());
    Options::setFitsPSFUpdate(m_LiveStackingUI.PSFUpdate->value());

    Options::setFitsLSPostProc(m_LiveStackingUI.PostProcGroupBox->isChecked());
    Options::setFitsLSDeconvAmt(m_LiveStackingUI.DeconvAmt->value());
    Options::setFitsLSPSFSigma(m_LiveStackingUI.PSFSigma->value());
    Options::setFitsLSDenoiseAmt(m_LiveStackingUI.DenoiseAmt->value());
    Options::setFitsLSSharpenAmt(m_LiveStackingUI.SharpenAmt->value());
    Options::setFitsLSSharpenKernal(m_LiveStackingUI.SharpenKernal->value());
    Options::setFitsLSSharpenSigma(m_LiveStackingUI.SharpenSigma->value());

    // Live Stacking Monitor
    if (m_View && m_View->getStackMon())
        m_View->getStackMon()->saveSettings();

    // Write the options to disk
    Options::self()->save();
    qCDebug(KSTARS_FITS) << "Live Stacker settings saved";
}

LiveStackData FITSTab::getAllSettings()
{
    LiveStackData data;
    data.calcSNR = m_LiveStackingUI.CalcSNR->isChecked();
    if (!m_StackMultiC)
    {
        data.masterDark.push_back(m_LiveStackingUI.MasterDark->text());
        data.masterFlat.push_back(m_LiveStackingUI.MasterFlat->text());
    }
    else
    {
        data.masterDark.push_back(m_LiveStackingUI.MasterDarkRed->text());
        data.masterDark.push_back(m_LiveStackingUI.MasterDarkGreen->text());
        data.masterDark.push_back(m_LiveStackingUI.MasterDarkBlue->text());
        data.masterDark.push_back(m_LiveStackingUI.MasterDarkLum->text());

        data.masterFlat.push_back(m_LiveStackingUI.MasterFlatRed->text());
        data.masterFlat.push_back(m_LiveStackingUI.MasterFlatGreen->text());
        data.masterFlat.push_back(m_LiveStackingUI.MasterFlatBlue->text());
        data.masterFlat.push_back(m_LiveStackingUI.MasterFlatLum->text());
    }
    data.hotPixels = m_LiveStackingUI.HotPixels->isChecked();
    data.coldPixels = m_LiveStackingUI.ColdPixels->isChecked();
    data.alignMaster = m_LiveStackingUI.AlignMaster->text();
    data.alignMethod = static_cast<LiveStackAlignMethod>(m_LiveStackingUI.AlignMethod->currentIndex());
    data.numInMem = m_LiveStackingUI.NumInMem->value();
    data.downscale = static_cast<LiveStackDownscale>(m_LiveStackingUI.LSDownscale->currentIndex());
    data.weighting = static_cast<LiveStackFrameWeighting>(m_LiveStackingUI.Weighting->currentIndex());
    data.stackingMethod = static_cast<LiveStackStackingMethod>(m_LiveStackingUI.StackingMethod->currentIndex());
    data.lowSigma = m_LiveStackingUI.LowSigma->value();
    data.highSigma = m_LiveStackingUI.HighSigma->value();
    data.windsorCutoff = m_LiveStackingUI.WinsorCutoff->value();
    data.iterations = m_LiveStackingUI.Iterations->value();
    data.kappa = m_LiveStackingUI.Kappa->value();
    data.alpha = m_LiveStackingUI.Alpha->value();
    data.sigma = m_LiveStackingUI.Sigma->value();
    data.PSFUpdate = m_LiveStackingUI.PSFUpdate->value();
    data.postProcessing = getPPSettings();
    return data;
}

LiveStackPPData FITSTab::getPPSettings()
{
    LiveStackPPData data;
    data.postProcess = m_LiveStackingUI.PostProcGroupBox->isChecked();
    data.deconvAmt = m_LiveStackingUI.DeconvAmt->value();
    data.PSFSigma = m_LiveStackingUI.PSFSigma->value();
    data.denoiseAmt = m_LiveStackingUI.DenoiseAmt->value();
    data.sharpenAmt = m_LiveStackingUI.SharpenAmt->value();
    data.sharpenKernal = m_LiveStackingUI.SharpenKernal->value();
    data.sharpenSigma = m_LiveStackingUI.SharpenSigma->value();
    return data;
}

void FITSTab::stackMethodChanged(int index)
{
    switch (static_cast<LiveStackStackingMethod>(index))
    {
        case LiveStackStackingMethod::MEAN:
            m_LiveStackingUI.LowSigma->hide();
            m_LiveStackingUI.LowSigmaLabel->hide();
            m_LiveStackingUI.HighSigma->hide();
            m_LiveStackingUI.HighSigmaLabel->hide();
            m_LiveStackingUI.WinsorCutoff->hide();
            m_LiveStackingUI.WinsorCutoffLabel->hide();
            m_LiveStackingUI.Iterations->hide();
            m_LiveStackingUI.IterationsLabel->hide();
            m_LiveStackingUI.Kappa->hide();
            m_LiveStackingUI.KappaLabel->hide();
            m_LiveStackingUI.Alpha->hide();
            m_LiveStackingUI.AlphaLabel->hide();
            m_LiveStackingUI.Sigma->hide();
            m_LiveStackingUI.SigmaLabel->hide();
            m_LiveStackingUI.PSFUpdate->hide();
            m_LiveStackingUI.PSFUpdateLabel->hide();
            break;
        case LiveStackStackingMethod::SIGMA:
            m_LiveStackingUI.LowSigma->show();
            m_LiveStackingUI.LowSigmaLabel->show();
            m_LiveStackingUI.HighSigma->show();
            m_LiveStackingUI.HighSigmaLabel->show();
            m_LiveStackingUI.WinsorCutoff->hide();
            m_LiveStackingUI.WinsorCutoffLabel->hide();
            m_LiveStackingUI.Iterations->hide();
            m_LiveStackingUI.IterationsLabel->hide();
            m_LiveStackingUI.Kappa->hide();
            m_LiveStackingUI.KappaLabel->hide();
            m_LiveStackingUI.Alpha->hide();
            m_LiveStackingUI.AlphaLabel->hide();
            m_LiveStackingUI.Sigma->hide();
            m_LiveStackingUI.SigmaLabel->hide();
            m_LiveStackingUI.PSFUpdate->hide();
            m_LiveStackingUI.PSFUpdateLabel->hide();
            break;
        case LiveStackStackingMethod::WINDSOR:
            m_LiveStackingUI.LowSigma->show();
            m_LiveStackingUI.LowSigmaLabel->show();
            m_LiveStackingUI.HighSigma->show();
            m_LiveStackingUI.HighSigmaLabel->show();
            m_LiveStackingUI.WinsorCutoff->show();
            m_LiveStackingUI.WinsorCutoffLabel->show();
            m_LiveStackingUI.Iterations->hide();
            m_LiveStackingUI.IterationsLabel->hide();
            m_LiveStackingUI.Kappa->hide();
            m_LiveStackingUI.KappaLabel->hide();
            m_LiveStackingUI.Alpha->hide();
            m_LiveStackingUI.AlphaLabel->hide();
            m_LiveStackingUI.Sigma->hide();
            m_LiveStackingUI.SigmaLabel->hide();
            m_LiveStackingUI.PSFUpdate->hide();
            m_LiveStackingUI.PSFUpdateLabel->hide();
            break;
        case LiveStackStackingMethod::IMAGEMM:
            m_LiveStackingUI.LowSigma->hide();
            m_LiveStackingUI.LowSigmaLabel->hide();
            m_LiveStackingUI.HighSigma->hide();
            m_LiveStackingUI.HighSigmaLabel->hide();
            m_LiveStackingUI.WinsorCutoff->hide();
            m_LiveStackingUI.WinsorCutoffLabel->hide();
            m_LiveStackingUI.Iterations->show();
            m_LiveStackingUI.IterationsLabel->show();
            m_LiveStackingUI.Kappa->show();
            m_LiveStackingUI.KappaLabel->show();
            m_LiveStackingUI.Alpha->show();
            m_LiveStackingUI.AlphaLabel->show();
            m_LiveStackingUI.Sigma->show();
            m_LiveStackingUI.SigmaLabel->show();
            m_LiveStackingUI.PSFUpdate->show();
            m_LiveStackingUI.PSFUpdateLabel->show();
            break;
        default:
            break;
    }
}

void FITSTab::redoPostProcessing()
{
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
    if(m_View)
    {
        m_LiveStackingUI.PostProcGroupBox->setEnabled(false);
        viewer->restack(getUID());
        m_View->redoPostProcessStack(getPPSettings());
    }
#endif // !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
}

void FITSTab::calcSNRChanged()
{
    m_LiveStackingUI.SubsSNR->setText("0 / 0 / 0");
    m_LiveStackingUI.ImageSNR->setText("0");
}

void FITSTab::launchLiveStackingHelp()
{
    const QUrl url("https://kstars-docs.kde.org/en/user_manual/fits-viewer-livestacker.html");
    if (!url.isEmpty())
        QDesktopServices::openUrl(url);
}

void FITSTab::selectLiveStack(QLineEdit *targetEdit, const QString &title)
{
    QFileDialog dialog(KStars::Instance(), i18nc("@title:window", "%1", title));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setDirectory(m_CurrentStackDir);

    if (dialog.exec())
    {
        const QStringList dirs = dialog.selectedFiles();
        if (dirs.isEmpty())
            return;

        const QString selectedDir = dirs[0];
        m_CurrentStackDir = selectedDir;

        if (targetEdit)
            targetEdit->setText(selectedDir);

        // Reset the align master if it doesn't match
        const QString alignMaster = m_LiveStackingUI.AlignMaster->text();
        if (!alignMaster.isEmpty())
        {
            QFileInfo info(alignMaster);
            if (info.absolutePath() != selectedDir)
                m_LiveStackingUI.AlignMaster->setText("");
        }
    }
}

void FITSTab::toggleMultiC()
{
    m_StackMultiC = !m_StackMultiC;

    // Single channel row
    m_LiveStackingUI.StackLabel->setVisible(!m_StackMultiC);
    m_LiveStackingUI.Stack->setVisible(!m_StackMultiC);
    m_LiveStackingUI.StackDirB->setVisible(!m_StackMultiC);
    m_LiveStackingUI.StackMultiCB1->setVisible(!m_StackMultiC);

    // Darks
    m_LiveStackingUI.MasterDarkLabel->setVisible(!m_StackMultiC);
    m_LiveStackingUI.MasterDark->setVisible(!m_StackMultiC);
    m_LiveStackingUI.MasterDarkB->setVisible(!m_StackMultiC);

    // Flats
    m_LiveStackingUI.MasterFlatLabel->setVisible(!m_StackMultiC);
    m_LiveStackingUI.MasterFlat->setVisible(!m_StackMultiC);
    m_LiveStackingUI.MasterFlatB->setVisible(!m_StackMultiC);

    // Multi-channel rows
    m_LiveStackingUI.RedLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.Red->setVisible(m_StackMultiC);
    m_LiveStackingUI.RedDirB->setVisible(m_StackMultiC);
    m_LiveStackingUI.StackMultiCB2->setVisible(m_StackMultiC);

    m_LiveStackingUI.GreenLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.Green->setVisible(m_StackMultiC);
    m_LiveStackingUI.GreenDirB->setVisible(m_StackMultiC);

    m_LiveStackingUI.BlueLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.Blue->setVisible(m_StackMultiC);
    m_LiveStackingUI.BlueDirB->setVisible(m_StackMultiC);

    m_LiveStackingUI.LumLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.Lum->setVisible(m_StackMultiC);
    m_LiveStackingUI.LumDirB->setVisible(m_StackMultiC);

    // Set some defaults. Nothing for single channel; for mult-channel copy the stack directory to Red
    if (m_StackMultiC && m_LiveStackingUI.Red->text().isEmpty())
        m_LiveStackingUI.Red->setText(m_LiveStackingUI.Stack->text());

    // Darks
    m_LiveStackingUI.MasterDarkRedLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkRed->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkRedB->setVisible(m_StackMultiC);

    m_LiveStackingUI.MasterDarkGreenLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkGreen->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkGreenB->setVisible(m_StackMultiC);

    m_LiveStackingUI.MasterDarkBlueLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkBlue->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkBlueB->setVisible(m_StackMultiC);

    m_LiveStackingUI.MasterDarkLumLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkLum->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterDarkLumB->setVisible(m_StackMultiC);

    // Flats
    m_LiveStackingUI.MasterFlatRedLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatRed->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatRedB->setVisible(m_StackMultiC);

    m_LiveStackingUI.MasterFlatGreenLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatGreen->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatGreenB->setVisible(m_StackMultiC);

    m_LiveStackingUI.MasterFlatBlueLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatBlue->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatBlueB->setVisible(m_StackMultiC);

    m_LiveStackingUI.MasterFlatLumLabel->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatLum->setVisible(m_StackMultiC);
    m_LiveStackingUI.MasterFlatLumB->setVisible(m_StackMultiC);

    stackDirChanged("");
}

void FITSTab::stackDirChanged(const QString &text)
{
    Q_UNUSED(text);
    bool cantStack;
    if (m_StackMultiC)
        // Can only stack in this mode if R, G and B are specified - L is optional
        cantStack = m_LiveStackingUI.Red->text().isEmpty() || m_LiveStackingUI.Green->text().isEmpty() ||
                    m_LiveStackingUI.Blue->text().isEmpty();
    else
        cantStack = m_LiveStackingUI.Stack->text().isEmpty();
    m_LiveStackingUI.StartB->setEnabled(!cantStack);
}

QString FITSTab::getStackDialogFilters()
{
    QStringList allFilters = STACK_FITS_FILTER + STACK_XISF_FILTER;
    QString allFilter = QString("All (%1)").arg(allFilters.join(' '));
    QString FITSFilter = QString("FITS (%1)").arg(STACK_FITS_FILTER.join(' '));
    QString XISFFilter = QString("XISF (%1)").arg(STACK_XISF_FILTER.join(' '));
    QString filter = allFilter + ";;" + FITSFilter + ";;" + XISFFilter;
    return allFilter + ";;" + FITSFilter + ";;" + XISFFilter;
}

void FITSTab::selectLiveStackAlignSub()
{
    QString file = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Select Alignment Sub"),
                   m_CurrentStackDir, getStackDialogFilters(), &m_StackLastFilter);

    if (!file.isNull())
    {
        QUrl sequenceURL = QUrl::fromLocalFile(file);
        m_LiveStackingUI.AlignMaster->setText(sequenceURL.toLocalFile());
        QFileInfo fileInfo(file);
        m_CurrentStackDir = fileInfo.absolutePath();
    }
}

void FITSTab::selectLiveStackMaster(QLineEdit *targetEdit, const QString &title)
{
    QString file = QFileDialog::getOpenFileName(this, i18nc("@title:window", "%1", title),
                   m_CurrentStackDir, getStackDialogFilters(), &m_StackLastFilter);
    if (!file.isNull())
    {
        QUrl sequenceURL = QUrl::fromLocalFile(file);
        if (targetEdit)
            targetEdit->setText(sequenceURL.toLocalFile());
        QFileInfo fileInfo(file);
        m_CurrentStackDir = fileInfo.absolutePath();
    }
}

void FITSTab::applyTypeFilter()
{
    if (!m_View)
        return;
    const QSharedPointer<FITSData> &imageData = m_View->imageData();
    if (!imageData)
        return;

    QStringList filters;
    QList<QTreeWidgetItem *> items = m_CatObjTypeFilterUI.tree->findItems("*",
                                     Qt::MatchWrap | Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto item : items)
    {
        if (item->checkState(0) == Qt::Checked)
        {
            if (!item->text(0).isEmpty())
                filters.push_back(item->text(0));
            if (!item->text(1).isEmpty())
                filters.push_back(item->text(1));
        }
    }

    // Store the new filter paradigm
    m_View->imageData()->setCatObjectsFilters(filters);
    // Process the data according to the new filter paradigm
    m_View->imageData()->filterCatObjects();
    m_View->updateFrame();
    // Reload FITSTab
    loadCatalogObjects();
}

void FITSTab::checkAllTypeFilter()
{
    QList<QTreeWidgetItem *> items = m_CatObjTypeFilterUI.tree->findItems("*",
                                     Qt::MatchWrap | Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto item : items)
    {
        item->setCheckState(0, Qt::Checked);
    }
}

void FITSTab::uncheckAllTypeFilter()
{
    QList<QTreeWidgetItem *> items = m_CatObjTypeFilterUI.tree->findItems("*",
                                     Qt::MatchWrap | Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto item : items)
    {
        item->setCheckState(0, Qt::Unchecked);
    }
}

void FITSTab::typeFilterItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0)
        return;

    Qt::CheckState check = item->checkState(column);
    for (int i = 0; i < item->childCount(); i++)
    {
        QTreeWidgetItem *child = item->child(i);
        child->setCheckState(column, check);
    }
}

void FITSTab::headerFITS()
{
    fitsTools->setCurrentIndex(2);
    if(m_View->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
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
        QString selectedFilter;
#ifdef Q_OS_MACOS //For some reason, the other code caused KStars to crash on MacOS
        currentURL =
            QFileDialog::getSaveFileUrl(KStars::Instance(), i18nc("@title:window", "Save FITS"), currentDir,
                                        "Images (*.fits *.fits.gz *.fit *.xisf *.jpg *.jpeg *.png)");
#else
        currentURL =
            QFileDialog::getSaveFileUrl(KStars::Instance(), i18nc("@title:window", "Save FITS"), currentDir,
                                        "FITS (*.fits *.fits.gz *.fit);;XISF (*.xisf);;JPEG (*.jpg *.jpeg);;PNG (*.png)", &selectedFilter);
#endif
        // if user presses cancel
        if (currentURL.isEmpty())
        {
            currentURL = backupCurrent;
            return false;
        }

        // If no extension is selected append one
        if (currentURL.toLocalFile().contains('.') == 0)
        {
            if (selectedFilter.contains("XISF"))
                currentURL.setPath(currentURL.toLocalFile() + ".xisf");
            else if (selectedFilter.contains("JPEG"))
                currentURL.setPath(currentURL.toLocalFile() + ".jpg");
            else if (selectedFilter.contains("PNG"))
                currentURL.setPath(currentURL.toLocalFile() + ".png");
            else
                currentURL.setPath(currentURL.toLocalFile() + ".fits");
        }
    }

    if (currentURL.isValid())
    {
        QString localFile = currentURL.toLocalFile();
        //        if (localFile.contains(".fit"))
        //            localFile = "!" + localFile;

        if (!saveImage(localFile))
        {
            KSNotification::error(i18n("Image save error: %1", m_View->imageData()->getLastError()), i18n("Image Save"));
            return false;
        }

        emit newStatus(i18n("File saved to %1", currentURL.url()), FITS_MESSAGE);
        modifyFITSState();
        return true;
    }
    else
    {
        QString message = i18n("Invalid URL: %1", currentURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
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
    QPoint oldCenter = m_View->getImagePoint(m_View->viewport()->rect().center());
    m_View->ZoomIn();
    m_View->cleanUpZoom(oldCenter);
}

void FITSTab::ZoomOut()
{
    QPoint oldCenter = m_View->getImagePoint(m_View->viewport()->rect().center());
    m_View->ZoomOut();
    m_View->cleanUpZoom(oldCenter);
}

void FITSTab::ZoomDefault()
{
    QPoint oldCenter = m_View->getImagePoint(m_View->viewport()->rect().center());
    m_View->ZoomDefault();
    m_View->cleanUpZoom(oldCenter);
}

void FITSTab::tabPositionUpdated()
{
    undoStack->setActive(true);
    m_View->emitZoom();
    emit newStatus(QString("%1x%2").arg(m_View->imageData()->width()).arg(m_View->imageData()->height()),
                   FITS_RESOLUTION);
}

void FITSTab::setStretchValues(double shadows, double midtones, double highlights)
{
    if (stretchUI)
        stretchUI->setStretchValues(shadows, midtones, highlights);
}

void FITSTab::setAutoStretch()
{
    if (!m_View->getAutoStretch())
        m_View->setAutoStretchParams();
}

void FITSTab::extractImage()
{
    connect(m_PlateSolve.data(), &PlateSolve::extractorSuccess, this, [this]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::extractorSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::extractorFailed, nullptr, nullptr);
        m_View->updateFrame();
        m_PlateSolve->solveImage(m_View->imageData());
    });
    connect(m_PlateSolve.data(), &PlateSolve::extractorFailed, this, [this]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::extractorSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::extractorFailed, nullptr, nullptr);
    });
    connect(m_PlateSolve.data(), &PlateSolve::solverFailed, this, [this]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::solverSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::solverFailed, nullptr, nullptr);
    });
    connect(m_PlateSolve.data(), &PlateSolve::solverSuccess, this, [this]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::solverSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::solverFailed, nullptr, nullptr);
        m_View->syncWCSState();
        if (m_View->areObjectsShown())
            // Requery Objects based on new plate solved solution
            m_View->imageData()->searchObjects();
    });
    m_PlateSolve->extractImage(m_View->imageData());
}

// Reload the live stack
void FITSTab::liveStack()
{
    QString text = m_LiveStackingUI.StartB->text().remove('&');
    if (text == TEXT_START.toString())
    {
        m_StackStarted = true;
        m_StackCancelled = false;

        // Start the stack process
        m_LiveStackingUI.StartB->setText(TEXT_STOP.toString());
        m_LiveStackingUI.StartB->setEnabled(true);
        m_LiveStackingUI.PostProcGroupBox->setEnabled(false);

        m_liveStackDir.clear();
        if (!m_StackMultiC)
            m_liveStackDir.push_back(m_LiveStackingUI.Stack->text());
        else
        {
            m_liveStackDir.push_back(m_LiveStackingUI.Red->text());
            m_liveStackDir.push_back(m_LiveStackingUI.Green->text());
            m_liveStackDir.push_back(m_LiveStackingUI.Blue->text());
            if (!m_LiveStackingUI.Lum->text().isEmpty())
                m_liveStackDir.push_back(m_LiveStackingUI.Lum->text());
        }

        // Setup default tabname (user can manually change and override)
        if (m_StackMultiC)
            m_StackDefTabName = m_LiveStackingUI.Lum->text().isEmpty() ? i18n("RGB") : i18n("LRGB");
        else
            m_StackDefTabName = m_liveStackDir.first();

        m_StackSubsTotal = 0;
        m_StackSubsProcessed = 0;
        m_StackSubsFailed = 0;
        m_LiveStackingUI.SubsProcessed->setText("0 / 0 / 0");
        m_LiveStackingUI.SubsSNR->setText("0 / 0 / 0");
        m_LiveStackingUI.ImageSNR->setText("0");
        viewer->restack(getUID());
        LiveStackData lsd = getAllSettings();
        m_View->loadStack(m_liveStackDir, lsd);

        qCInfo(KSTARS_FITS).nospace()
                << "Starting Live Stacker (" << (m_StackMultiC ? "Multi" : "Single") << " channel) on "
                << " | Dir(s): " << m_liveStackDir.join(", ")
                << " | Calc SNR: " << (lsd.calcSNR ? "On" : "Off")
                << " | AlignMaster: " << (lsd.alignMaster.isEmpty() ? "None" : lsd.alignMaster)
                << " | AlignMethod: " << LiveStackAlignMethodNames.value(lsd.alignMethod)
                << " | MasterDark(s): " << (lsd.masterDark.isEmpty() ? "None" :
                                            QStringList::fromVector(lsd.masterDark).join(", "))
                << " | MasterFlat(s): " << (lsd.masterFlat.isEmpty() ? "None" :
                                            QStringList::fromVector(lsd.masterFlat).join(", "))
                << " | Hot Pixels: " << (lsd.hotPixels ? "On" : "Off")
                << " | Cold Pixels: " << (lsd.coldPixels ? "On" : "Off")
                << " | Downscale: " << LiveStackDownscaleNames.value(lsd.downscale)
                << " | Weighting: " << LiveStackFrameWeightingNames.value(lsd.weighting)
                << " | StackMethod: " << LiveStackStackingMethodNames.value(lsd.stackingMethod)
                << " | Sigma(low/high): " << lsd.lowSigma << "/" << lsd.highSigma
                << " | WindsorCutoff: " << lsd.windsorCutoff
                << " | Iterations: " << lsd.iterations
                << " | Kappa: " << lsd.kappa
                << " | Alpha: " << lsd.alpha
                << " | Sigma: " << lsd.sigma
                << " | PSF Update: " << lsd.PSFUpdate
                << " | NumInMem: " << lsd.numInMem
                << " | PostProc: " << (lsd.postProcessing.postProcess ? "On" : "Off")
                << " [Deconv=" << lsd.postProcessing.deconvAmt
                << ", PSFSigma=" << lsd.postProcessing.PSFSigma
                << ", Denoise=" << lsd.postProcessing.denoiseAmt
                << ", Sharpen=" << lsd.postProcessing.sharpenAmt
                << ", Kernel=" << lsd.postProcessing.sharpenKernal
                << ", Sigma=" << lsd.postProcessing.sharpenSigma << "]";
    }
    else if (text == TEXT_STOP.toString())
    {
        m_LiveStackingUI.StartB->setText(TEXT_STOPPING.toString());
        m_LiveStackingUI.StartB->setEnabled(false);
        m_LiveStackingUI.PostProcGroupBox->setEnabled(false);
        m_View->cancelStack();
        qCInfo(KSTARS_FITS).nospace() << "Stopping Live Stacker";
    }
}

// Plate solve the sub. May require 2 runs; firstly to get stars (if needed) and then to actually plate solve
// Also, we assume that once the first sub is solved subsequent subs can be solved on just the same index and HEALPix
// This significantly speeds up plate solving but if that solve fails... we widen the criteria and try again
// before completely giving up on the sub
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
void FITSTab::plateSolveSub(const double ra, const double dec, const double pixScale, const int index,
                            const int healpix, const LiveStackFrameWeighting &weighting)
{
    connect(m_PlateSolve.data(), &PlateSolve::subExtractorSuccess, this, [this, ra, dec, pixScale, index, healpix]
            (double medianHFR, int numStars)
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorFailed, nullptr, nullptr);
        m_StackMedianHFR = medianHFR;
        m_StackNumStars = numStars;
        qCDebug(KSTARS_FITS) << "Star extraction complete, plate solving starting...";
        m_PlateSolve->plateSolveSub(m_View->imageData(), ra, dec, pixScale, index, healpix, SSolver::SOLVE);
    });
    connect(m_PlateSolve.data(), &PlateSolve::subExtractorFailed, this, [this]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorFailed, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subSolverSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subSolverFailed, nullptr, nullptr);
        const bool timedOut = false;
        const bool success = false;
        m_StackMedianHFR = -1.0;
        m_StackNumStars = 0;
        m_View->imageData()->solverDone(timedOut, success, m_StackMedianHFR, m_StackNumStars);
    });
    connect(m_PlateSolve.data(), &PlateSolve::subSolverFailed, this, [this, ra, dec, pixScale]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorFailed, nullptr, nullptr);
        if (m_StackExtendedPlateSolve)
        {
            // Failed to plate solve on extended criteria so just fail
            disconnect(m_PlateSolve.data(), &PlateSolve::subSolverSuccess, nullptr, nullptr);
            disconnect(m_PlateSolve.data(), &PlateSolve::subSolverFailed, nullptr, nullptr);
            const bool timedOut = false;
            const bool success = false;
            m_View->imageData()->solverDone(timedOut, success, m_StackMedianHFR, m_StackNumStars);
        }
        else
        {
            // Failed to plate solve on tight criteria so have another go on widened criteria
            m_StackExtendedPlateSolve = true;
            qCDebug(KSTARS_FITS) << "Plate solving failed, extending criteria and retrying...";
            m_PlateSolve->plateSolveSub(m_View->imageData(), ra, dec, pixScale, -1, -1, SSolver::SOLVE);
        }
    });
    connect(m_PlateSolve.data(), &PlateSolve::subSolverSuccess, this, [this]()
    {
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subExtractorFailed, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subSolverSuccess, nullptr, nullptr);
        disconnect(m_PlateSolve.data(), &PlateSolve::subSolverFailed, nullptr, nullptr);
        const bool timedOut = false;
        const bool success = true;
        m_View->imageData()->solverDone(timedOut, success, m_StackMedianHFR, m_StackNumStars);
    });

    SSolver::ProcessType solveType;

    if (weighting == LiveStackFrameWeighting::HFR)
        solveType = SSolver::EXTRACT_WITH_HFR;
    else if (weighting == LiveStackFrameWeighting::NUM_STARS)
        solveType = SSolver::EXTRACT;
    else
        solveType = SSolver::SOLVE;

    m_StackExtendedPlateSolve = (index == -1);
    m_PlateSolve->plateSolveSub(m_View->imageData(), ra, dec, pixScale, index, healpix, solveType);
}
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV

void FITSTab::stackInProgress()
{
    m_LiveStackingUI.PostProcGroupBox->setEnabled(false);
    viewer->restack(getUID());
}

void FITSTab::alignMasterChosen(const QString &alignMaster)
{
    m_LiveStackingUI.AlignMaster->setText(alignMaster);
}

void FITSTab::stackUpdateStats(const bool ok, const int sub, const int total, const double meanSNR, const double minSNR,
                               const double maxSNR)
{
    Q_UNUSED(sub);
    m_StackSubsTotal = total;
    (ok) ? m_StackSubsProcessed++ : m_StackSubsFailed++;
    m_LiveStackingUI.SubsProcessed->setText(QString("%1 / %2 / %3").arg(m_StackSubsProcessed).arg(m_StackSubsFailed).arg(
            m_StackSubsTotal));
    m_LiveStackingUI.SubsSNR->setText(QString("%1 / %2 / %3").arg(meanSNR, 0, 'f', 2).arg(minSNR, 0, 'f', 2).arg(maxSNR, 0, 'f',
                                      2));
}

QString FITSTab::getTabTitle() const
{
    QString title = (m_View && (m_View->getMode() == FITS_LIVESTACKING)) ? "Stack" : "Image";
    if (!m_StackStarted && !m_TabName.isEmpty())
        title = m_TabName;
    else if (m_StackStarted && m_TabName.isEmpty())
    {
        QString start = (m_StackCancelled) ? i18n("(%1) Stopped Watching", m_StackSubsProcessed) :
                        i18n("(%1) Watching", m_StackSubsProcessed);
        title = i18n("%1 %2", start, m_StackDefTabName);
    }
    else if (m_StackStarted && !m_TabName.isEmpty())
    {
        QString start = (m_StackCancelled) ? i18n("(%1) Stopped", m_StackSubsProcessed) :
                        i18n("(%1)", m_StackSubsProcessed);
        title = i18n("%1 %2", start, m_TabName);
    }
    return title;
}

void FITSTab::updateStackSNR(const double SNR)
{
    m_LiveStackingUI.ImageSNR->setText(QString("%1").arg(SNR, 0, 'f', 2));
}

void FITSTab::resetStack(const bool cancelled)
{
    if (cancelled)
    {
        // Cancel request has been actioned
        m_LiveStackingUI.StartB->setText(TEXT_START.toString());
        m_LiveStackingUI.StartB->setEnabled(true);
        m_LiveStackingUI.PostProcGroupBox->setEnabled(true);
        m_StackCancelled = true;
        viewer->stackCancelled(getUID());
    }
    else if (m_StackSubsTotal <= m_StackSubsProcessed + m_StackSubsFailed)
    {
        // If stacking is complete make action buttons active
        m_LiveStackingUI.StartB->setEnabled(true);
        m_LiveStackingUI.PostProcGroupBox->setEnabled(true);
    }
}
