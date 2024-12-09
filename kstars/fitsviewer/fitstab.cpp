/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitstab.h"

#include "QtNetwork/qnetworkreply.h"
#include "auxiliary/kspaths.h"
#include "fitsdata.h"
#include "fitshistogramcommand.h"
#include "fitsview.h"
#include "fitsviewer.h"
#include "ksnotification.h"
#include "kstars.h"
#include "Options.h"
#include "ui_fitsheaderdialog.h"
#include "ui_statform.h"
#include "fitsstretchui.h"
#include "skymap.h"
#include <KMessageBox>
#include <QFileDialog>
#include <QClipboard>
#include <QIcon>
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"

#include <fits_debug.h>
#include "fitscommon.h"
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>

QPointer<Ekos::StellarSolverProfileEditor> FITSTab::m_ProfileEditor;
QPointer<KConfigDialog> FITSTab::m_EditorDialog;
QPointer<KPageWidgetItem> FITSTab::m_ProfileEditorPage;

constexpr int CAT_OBJ_SORT_ROLE = Qt::UserRole + 1;

FITSTab::FITSTab(FITSViewer *parent) : QWidget(parent)
{
    viewer    = parent;
    undoStack = new QUndoStack(this);
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));

    m_PlateSolveWidget = new QDialog(this);
    m_CatalogObjectWidget = new QDialog(this);
    statWidget = new QDialog(this);
    fitsHeaderDialog = new QDialog(this);
    m_HistogramEditor = new FITSHistogramEditor(this);
    connect(m_HistogramEditor, &FITSHistogramEditor::newHistogramCommand, this, [this](FITSHistogramCommand * command)
    {
        undoStack->push(command);
    });
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
        m_View.reset(new FITSView(this, mode, filter));
        m_View->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *vlayout = new QVBoxLayout();

        connect(m_View.get(), &FITSView::rectangleUpdated, this, [this](QRect roi)
        {
            displayStats(roi.isValid());
        });
        fitsSplitter = new QSplitter(Qt::Horizontal, this);
        fitsTools = new QToolBox();

        stat.setupUi(statWidget);
        m_PlateSolveUI.setupUi(m_PlateSolveWidget);

        m_PlateSolveUI.editProfile->setIcon(QIcon::fromTheme("document-edit"));
        m_PlateSolveUI.editProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

        const QString EditorID = "FITSSolverProfileEditor";
        if (!m_EditorDialog)
        {
            // These are static, shared by all FITS Viewer tabs.
            m_EditorDialog = new KConfigDialog(nullptr, EditorID, Options::self());
            m_ProfileEditor = new Ekos::StellarSolverProfileEditor(nullptr, Ekos::AlignProfiles, m_EditorDialog.data());
            m_ProfileEditorPage = m_EditorDialog->addPage(m_ProfileEditor.data(),
                                  i18n("FITS Viewer Solver Profiles Editor"));
        }

        connect(m_PlateSolveUI.editProfile, &QAbstractButton::clicked, this, [this, EditorID]
        {
            m_ProfileEditor->loadProfile(m_PlateSolveUI.kcfg_FitsSolverProfile->currentText());
            KConfigDialog * d = KConfigDialog::exists(EditorID);
            if(d)
            {
                d->setCurrentPage(m_ProfileEditorPage);
                d->show();
            }
        });

        connect(m_PlateSolveUI.SolveButton, &QPushButton::clicked, this, &FITSTab::extractImage);

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

        fitsTools->addItem(statWidget, i18n("Statistics"));

        fitsTools->addItem(m_PlateSolveWidget, i18n("Plate Solving"));
        initSolverUI();

        // Setup the Catalog Object page
        m_CatalogObjectUI.setupUi(m_CatalogObjectWidget);
        m_CatalogObjectItem = fitsTools->addItem(m_CatalogObjectWidget, i18n("Catalog Objects"));
        initCatalogObject();

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

namespace
{
const QList<SSolver::Parameters> getSSolverParametersList(Ekos::ProfileGroup module)
{
    QString savedProfiles;
    switch(module)
    {
        case Ekos::AlignProfiles:
        default:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultAlignOptionsProfiles();
            break;
        case Ekos::FocusProfiles:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedFocusProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultFocusOptionsProfiles();
            break;
        case Ekos::GuideProfiles:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedGuideProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultGuideOptionsProfiles();
            break;
        case Ekos::HFRProfiles:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedHFRProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultHFROptionsProfiles();
            break;
    }
}
} // namespace

void FITSTab::setupSolver(bool extractOnly)
{
    auto parameters = getSSolverParametersList(static_cast<Ekos::ProfileGroup>(Options::fitsSolverModule())).at(
                          m_PlateSolveUI.kcfg_FitsSolverProfile->currentIndex());
    parameters.search_radius = m_PlateSolveUI.kcfg_FitsSolverRadius->value();
    if (extractOnly)
    {
        if (!m_PlateSolveUI.kcfg_FitsSolverLinear->isChecked())
        {
            // If image is non-linear seed the threshold offset with the background using median pixel value. Note
            // that there is a bug in the median calculation due to an issue compiling on Mac that means that not
            // all datatypes are supported by the median calculation. If median is zero use the mean instead.
            double offset = m_View->imageData()->getAverageMedian();
            if (offset <= 0.0)
                offset = m_View->imageData()->getAverageMean();
            parameters.threshold_offset = offset;
        }

        m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::EXTRACT),  &QObject::deleteLater);
        connect(m_Solver.get(), &SolverUtils::done, this, &FITSTab::extractorDone, Qt::UniqueConnection);
    }
    else
    {
        // If image is non-linear then set the offset to the average background in the image
        // which was found in the first solver (extract only) run.
        if (m_Solver && !m_PlateSolveUI.kcfg_FitsSolverLinear->isChecked())
            parameters.threshold_offset = m_Solver->getBackground().global;

        m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::SOLVE),  &QObject::deleteLater);
        connect(m_Solver.get(), &SolverUtils::done, this, &FITSTab::solverDone, Qt::UniqueConnection);
    }

    const int imageWidth = m_View->imageData()->width();
    const int imageHeight = m_View->imageData()->height();
    if (m_PlateSolveUI.kcfg_FitsSolverUseScale->isChecked() && imageWidth != 0 && imageHeight != 0)
    {
        const double scale = m_PlateSolveUI.kcfg_FitsSolverScale->value();
        double lowScale = scale * 0.8;
        double highScale = scale * 1.2;

        // solver utils uses arcsecs per pixel only
        const int units = m_PlateSolveUI.kcfg_FitsSolverImageScaleUnits->currentIndex();
        if (units == SSolver::DEG_WIDTH)
        {
            lowScale = (lowScale * 3600) / std::max(imageWidth, imageHeight);
            highScale = (highScale * 3600) / std::min(imageWidth, imageHeight);
        }
        else if (units == SSolver::ARCMIN_WIDTH)
        {
            lowScale = (lowScale * 60) / std::max(imageWidth, imageHeight);
            highScale = (highScale * 60) / std::min(imageWidth, imageHeight);
        }

        m_Solver->useScale(m_PlateSolveUI.kcfg_FitsSolverUseScale->isChecked(), lowScale, highScale);
    }
    else m_Solver->useScale(false, 0, 0);

    if (m_PlateSolveUI.kcfg_FitsSolverUsePosition->isChecked())
    {
        bool ok;
        const dms ra = m_PlateSolveUI.FitsSolverEstRA->createDms(&ok);
        bool ok2;
        const dms dec = m_PlateSolveUI.FitsSolverEstDec->createDms(&ok2);
        if (ok && ok2)
            m_Solver->usePosition(true, ra.Degrees(), dec.Degrees());
        else
            m_Solver->usePosition(false, 0, 0);
    }
    else m_Solver->usePosition(false, 0, 0);
}

// If it is currently solving an image, then cancel the solve.
// Otherwise start solving.
void FITSTab::extractImage()
{
    if (m_Solver.get() && m_Solver->isRunning())
    {
        m_PlateSolveUI.SolveButton->setText(i18n("Aborting..."));
        m_Solver->abort();
        return;
    }
    m_PlateSolveUI.SolveButton->setText(i18n("Cancel"));

    setupSolver(true);

    m_PlateSolveUI.FitsSolverAngle->setText("");
    m_PlateSolveUI.FitsSolverIndexfile->setText("");
    m_PlateSolveUI.Solution1->setText(i18n("Extracting..."));
    m_PlateSolveUI.Solution2->setText("");

    m_Solver->runSolver(m_View->imageData());
}

void FITSTab::solveImage()
{
    if (m_Solver.get() && m_Solver->isRunning())
    {
        m_PlateSolveUI.SolveButton->setText(i18n("Aborting..."));
        m_Solver->abort();
        return;
    }
    m_PlateSolveUI.SolveButton->setText(i18n("Cancel"));

    setupSolver(false);

    m_PlateSolveUI.Solution2->setText(i18n("Solving..."));

    m_Solver->runSolver(m_View->imageData());
}

void FITSTab::extractorDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    Q_UNUSED(solution);
    disconnect(m_Solver.get(), &SolverUtils::done, this, &FITSTab::extractorDone);
    m_PlateSolveUI.Solution2->setText("");

    if (timedOut)
    {
        const QString result = i18n("Extractor timed out: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        m_PlateSolveUI.Solution1->setText(result);

        // Can't run the solver. Just reset.
        m_PlateSolveUI.SolveButton->setText("Solve");
        return;
    }
    else if (!success)
    {
        const QString result = i18n("Extractor failed: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        m_PlateSolveUI.Solution1->setText(result);

        // Can't run the solver. Just reset.
        m_PlateSolveUI.SolveButton->setText(i18n("Solve"));
        return;
    }
    else
    {
        const QString starStr = i18n("Extracted %1 stars (%2 unfiltered) in %3s",
                                     m_Solver->getNumStarsFound(),
                                     m_Solver->getBackground().num_stars_detected,
                                     QString("%1").arg(elapsedSeconds, 0, 'f', 1));
        m_PlateSolveUI.Solution1->setText(starStr);

        // Set the stars in the FITSData object so the user can view them.
        const QList<FITSImage::Star> &starList = m_Solver->getStarList();
        QList<Edge*> starCenters;
        starCenters.reserve(starList.size());
        for (int i = 0; i < starList.size(); i++)
        {
            const auto &star = starList[i];
            Edge *oneEdge = new Edge();
            oneEdge->x = star.x;
            oneEdge->y = star.y;
            oneEdge->val = star.peak;
            oneEdge->sum = star.flux;
            oneEdge->HFR = star.HFR;
            oneEdge->width = star.a;
            oneEdge->numPixels = star.numPixels;
            if (star.a > 0)
                // See page 63 to find the ellipticity equation for SEP.
                // http://astroa.physics.metu.edu.tr/MANUALS/sextractor/Guide2source_extractor.pdf
                oneEdge->ellipticity = 1 - star.b / star.a;
            else
                oneEdge->ellipticity = 0;

            starCenters.append(oneEdge);
        }
        m_View->imageData()->setStarCenters(starCenters);
        m_View->updateFrame();

        // Now run the solver.
        solveImage();
    }
}

void FITSTab::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &FITSTab::solverDone);
    m_PlateSolveUI.SolveButton->setText("Solve");

    if (m_Solver->isRunning())
        qCDebug(KSTARS_FITS) << "solverDone called, but it is still running.";

    if (timedOut)
    {
        const QString result = i18n("Solver timed out: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        m_PlateSolveUI.Solution2->setText(result);
    }
    else if (!success)
    {
        const QString result = i18n("Solver failed: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        m_PlateSolveUI.Solution2->setText(result);
    }
    else
    {
        const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
        m_View->imageData()->injectWCS(solution.orientation, solution.ra, solution.dec, solution.pixscale, eastToTheRight);
        m_View->imageData()->loadWCS();
        m_View->syncWCSState();
        if (m_View->areObjectsShown())
            // Requery Objects based on new plate solved solution
            m_View->imageData()->searchObjects();

        const QString result = QString("Solved in %1s").arg(elapsedSeconds, 0, 'f', 1);
        const double solverPA = KSUtils::rotationToPositionAngle(solution.orientation);
        m_PlateSolveUI.FitsSolverAngle->setText(QString("%1º").arg(solverPA, 0, 'f', 2));

        int indexUsed = -1, healpixUsed = -1;
        m_Solver->getSolutionHealpix(&indexUsed, &healpixUsed);
        if (indexUsed < 0)
            m_PlateSolveUI.FitsSolverIndexfile->setText("");
        else
            m_PlateSolveUI.FitsSolverIndexfile->setText(
                QString("%1%2")
                .arg(indexUsed)
                .arg(healpixUsed >= 0 ? QString("-%1").arg(healpixUsed) : QString("")));;

        // Set the scale widget to the current result
        const int imageWidth = m_View->imageData()->width();
        const int units = m_PlateSolveUI.kcfg_FitsSolverImageScaleUnits->currentIndex();
        if (units == SSolver::DEG_WIDTH)
            m_PlateSolveUI.kcfg_FitsSolverScale->setValue(solution.pixscale * imageWidth / 3600.0);
        else if (units == SSolver::ARCMIN_WIDTH)
            m_PlateSolveUI.kcfg_FitsSolverScale->setValue(solution.pixscale * imageWidth / 60.0);
        else
            m_PlateSolveUI.kcfg_FitsSolverScale->setValue(solution.pixscale);

        // Set the ra and dec widgets to the current result
        m_PlateSolveUI.FitsSolverEstRA->show(dms(solution.ra));
        m_PlateSolveUI.FitsSolverEstDec->show(dms(solution.dec));

        m_PlateSolveUI.Solution2->setText(result);
    }
}

// Each module can default to its own profile index. These two methods retrieves and saves
// the values in a JSON string using an Options variable.
int FITSTab::getProfileIndex(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= Ekos::ProfileGroupNames.size())
        return 0;
    const QString moduleName = Ekos::ProfileGroupNames[moduleIndex].toString();
    const QString str = Options::fitsSolverProfileIndeces();
    const QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
    if (doc.isNull() || !doc.isObject())
        return 0;
    const QJsonObject indeces = doc.object();
    return indeces[moduleName].toString().toInt();
}

void FITSTab::setProfileIndex(int moduleIndex, int profileIndex)
{
    if (moduleIndex < 0 || moduleIndex >= Ekos::ProfileGroupNames.size())
        return;
    QString str = Options::fitsSolverProfileIndeces();
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
    if (doc.isNull() || !doc.isObject())
    {
        QJsonObject initialIndeces;
        for (int i = 0; i < Ekos::ProfileGroupNames.size(); i++)
        {
            QString name = Ekos::ProfileGroupNames[i].toString();
            if (name == "Align")
                initialIndeces[name] = QString::number(Options::solveOptionsProfile());
            else if (name == "Guide")
                initialIndeces[name] = QString::number(Options::guideOptionsProfile());
            else if (name == "HFR")
                initialIndeces[name] = QString::number(Options::hFROptionsProfile());
            else // Focus has a weird setting, just default to 0
                initialIndeces[name] = "0";
        }
        doc = QJsonDocument(initialIndeces);
    }

    QJsonObject indeces = doc.object();
    indeces[Ekos::ProfileGroupNames[moduleIndex].toString()] = QString::number(profileIndex);
    doc = QJsonDocument(indeces);
    Options::setFitsSolverProfileIndeces(QString(doc.toJson()));
}

void FITSTab::setupProfiles(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= Ekos::ProfileGroupNames.size())
        return;
    Ekos::ProfileGroup profileGroup = static_cast<Ekos::ProfileGroup>(moduleIndex);
    Options::setFitsSolverModule(moduleIndex);

    // Set up the profiles' menu.
    const QList<SSolver::Parameters> optionsList = getSSolverParametersList(profileGroup);
    m_PlateSolveUI.kcfg_FitsSolverProfile->clear();
    for(auto &param : optionsList)
        m_PlateSolveUI.kcfg_FitsSolverProfile->addItem(param.listName);

    m_ProfileEditor->setProfileGroup(profileGroup, false);

    // Restore the stored options.
    m_PlateSolveUI.kcfg_FitsSolverProfile->setCurrentIndex(getProfileIndex(Options::fitsSolverModule()));

    m_ProfileEditorPage->setHeader(QString("FITS Viewer Solver %1 Profiles Editor")
                                   .arg(Ekos::ProfileGroupNames[moduleIndex].toString()));
}

void FITSTab::initSolverUI()
{
    // Init the modules combo box.
    m_PlateSolveUI.kcfg_FitsSolverModule->clear();
    for (int i = 0; i < Ekos::ProfileGroupNames.size(); i++)
        m_PlateSolveUI.kcfg_FitsSolverModule->addItem(Ekos::ProfileGroupNames[i].toString());
    m_PlateSolveUI.kcfg_FitsSolverModule->setCurrentIndex(Options::fitsSolverModule());

    setupProfiles(Options::fitsSolverModule());

    // Change the profiles combo box whenever the modules combo changes
    connect(m_PlateSolveUI.kcfg_FitsSolverModule, QOverload<int>::of(&QComboBox::activated), this, &FITSTab::setupProfiles);

    m_PlateSolveUI.kcfg_FitsSolverUseScale->setChecked(Options::fitsSolverUseScale());
    m_PlateSolveUI.kcfg_FitsSolverScale->setValue(Options::fitsSolverScale());
    m_PlateSolveUI.kcfg_FitsSolverImageScaleUnits->setCurrentIndex(Options::fitsSolverImageScaleUnits());

    m_PlateSolveUI.kcfg_FitsSolverUsePosition->setChecked(Options::fitsSolverUsePosition());
    m_PlateSolveUI.kcfg_FitsSolverRadius->setValue(Options::fitsSolverRadius());

    m_PlateSolveUI.FitsSolverEstRA->setUnits(dmsBox::HOURS);
    m_PlateSolveUI.FitsSolverEstDec->setUnits(dmsBox::DEGREES);

    // Save the values of user controls when the user changes them.
    connect(m_PlateSolveUI.kcfg_FitsSolverProfile, QOverload<int>::of(&QComboBox::activated), [this](int index)
    {
        setProfileIndex(m_PlateSolveUI.kcfg_FitsSolverModule->currentIndex(), index);
    });

    connect(m_PlateSolveUI.kcfg_FitsSolverUseScale, &QCheckBox::stateChanged, this, [](int state)
    {
        Options::setFitsSolverUseScale(state);
    });
    connect(m_PlateSolveUI.kcfg_FitsSolverScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [](double value)
    {
        Options::setFitsSolverScale(value);
    });
    connect(m_PlateSolveUI.kcfg_FitsSolverImageScaleUnits, QOverload<int>::of(&QComboBox::activated), [](int index)
    {
        Options::setFitsSolverImageScaleUnits(index);
    });

    connect(m_PlateSolveUI.kcfg_FitsSolverUsePosition, &QCheckBox::stateChanged, this, [](int state)
    {
        Options::setFitsSolverUsePosition(state);
    });

    connect(m_PlateSolveUI.kcfg_FitsSolverRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value)
    {
        Options::setFitsSolverRadius(value);
    });
    connect(m_PlateSolveUI.UpdatePosition, &QPushButton::clicked, this, [&]()
    {
        const auto center = SkyMap::Instance()->getCenterPoint();
        m_PlateSolveUI.FitsSolverEstRA->show(center.ra());
        m_PlateSolveUI.FitsSolverEstDec->show(center.dec());
    });

    // Warn if the user is not using the internal StellarSolver solver.
    const SSolver::SolverType type = static_cast<SSolver::SolverType>(Options::solverType());
    if(type != SSolver::SOLVER_STELLARSOLVER)
    {
        m_PlateSolveUI.Solution2->setText(i18n("Warning! This tool only supports the internal StellarSolver solver."));
        m_PlateSolveUI.Solution1->setText(i18n("Change to that in the Ekos Align options menu."));
    }
}
