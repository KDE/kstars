/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitstab.h"

#include "auxiliary/kspaths.h"
#include "fitsdata.h"
#include "fitshistogrameditor.h"
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
#include <QtConcurrent>
#include <QIcon>
#include "ekos/auxiliary/stellarsolverprofile.h"

#include <fits_debug.h>

FITSTab::FITSTab(FITSViewer *parent) : QWidget(parent)
{
    viewer    = parent;
    undoStack = new QUndoStack(this);
    undoStack->setUndoLimit(10);
    undoStack->clear();
    connect(undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(modifyFITSState(bool)));

    m_PlateSolveWidget = new QDialog(this);
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

    int ans = KMessageBox::warningYesNoCancel(nullptr, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard());
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

        fitsSplitter = new QSplitter(Qt::Horizontal, this);
        fitsTools = new QToolBox();

        stat.setupUi(statWidget);
        m_PlateSolveUI.setupUi(m_PlateSolveWidget);

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

        fitsTools->addItem(m_HistogramEditor, i18n("Histogram"));

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
        return;

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

    evaluateStats();

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
    //    if (Options::nonLinearHistogram())
    //    {
    //        m_HistogramEditor->createNonLinearHistogram();
    //        evaluateStats();
    //    }
    //    if (!m_View->imageData()->isHistogramConstructed())
    //    {
    //        m_View->imageData()->constructHistogram();
    //        evaluateStats();
    //    }

    fitsTools->setCurrentIndex(1);
    if(m_View->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
}

void FITSTab::evaluateStats()
{
    const QSharedPointer<FITSData> &imageData = m_View->imageData();

    stat.statsTable->item(STAT_WIDTH, 0)->setText(QString::number(imageData->width()));
    stat.statsTable->item(STAT_HEIGHT, 0)->setText(QString::number(imageData->height()));
    stat.statsTable->item(STAT_BITPIX, 0)->setText(QString::number(imageData->bpp()));
    stat.statsTable->item(STAT_HFR, 0)->setText(QString::number(imageData->getHFR(), 'f', 3));

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
        stat.statsTable->item(STAT_MIN, i)->setText(QString::number(imageData->getMin(i), 'f', 3));
        stat.statsTable->item(STAT_MAX, i)->setText(QString::number(imageData->getMax(i), 'f', 3));
        stat.statsTable->item(STAT_MEAN, i)->setText(QString::number(imageData->getMean(i), 'f', 3));
        stat.statsTable->item(STAT_MEDIAN, i)->setText(QString::number(imageData->getMedian(i), 'f', 3));
        stat.statsTable->item(STAT_STDDEV, i)->setText(QString::number(imageData->getStdDev(i), 'f', 3));
    }
}

void FITSTab::statFITS()
{
    fitsTools->setCurrentIndex(0);
    if(m_View->width() > 200)
        fitsSplitter->setSizes(QList<int>() << 200 << m_View->width() - 200);
    else
        fitsSplitter->setSizes(QList<int>() << 50 << 50);
    evaluateStats();
}

void FITSTab::loadFITSHeader()
{
    const QSharedPointer<FITSData> &imageData = m_View->imageData();

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
#ifdef Q_OS_OSX //For some reason, the other code caused KStars to crash on MacOS
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
    emit newStatus(QString("%1%").arg(m_View->getCurrentZoom()), FITS_ZOOM);
    emit newStatus(QString("%1x%2").arg(m_View->imageData()->width()).arg(m_View->imageData()->height()),
                   FITS_RESOLUTION);
}

void FITSTab::setStretchValues(double shadows, double midtones, double highlights)
{
    if (stretchUI)
        stretchUI->setStretchValues(shadows, midtones, highlights);
}

namespace
{
const QList<SSolver::Parameters> getSSolverParametersList()
{
    const QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");

    return QFile(savedOptionsProfiles).exists() ?
           StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles) :
           Ekos::getDefaultAlignOptionsProfiles();
}
} // namespace

void FITSTab::setupSolver(bool extractOnly)
{
    auto parameters = getSSolverParametersList().at(m_PlateSolveUI.kcfg_FitsSolverProfile->currentIndex());
    parameters.search_radius = m_PlateSolveUI.kcfg_FitsSolverRadius->value();
    if (extractOnly)
    {
        m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::EXTRACT),  &QObject::deleteLater);
        connect(m_Solver.get(), &SolverUtils::done, this, &FITSTab::extractorDone, Qt::UniqueConnection);
    }
    else
    {
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

        const QString result = QString("Solved in %1s").arg(elapsedSeconds, 0, 'f', 1);
        const double solverPA = KSUtils::rotationToPositionAngle(solution.orientation);
        m_PlateSolveUI.FitsSolverAngle->setText(QString("%1º").arg(solverPA, 0, 'f', 2));

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

void FITSTab::initSolverUI()
{
    // Init the profiles combo box.
    const QList<SSolver::Parameters> optionsList = getSSolverParametersList();
    m_PlateSolveUI.kcfg_FitsSolverProfile->clear();
    for(auto &param : optionsList)
        m_PlateSolveUI.kcfg_FitsSolverProfile->addItem(param.listName);

    // Restore the stored options.
    m_PlateSolveUI.kcfg_FitsSolverProfile->setCurrentIndex(Options::fitsSolverProfile());

    m_PlateSolveUI.kcfg_FitsSolverUseScale->setChecked(Options::fitsSolverUseScale());
    m_PlateSolveUI.kcfg_FitsSolverScale->setValue(Options::fitsSolverScale());
    m_PlateSolveUI.kcfg_FitsSolverImageScaleUnits->setCurrentIndex(Options::fitsSolverImageScaleUnits());

    m_PlateSolveUI.kcfg_FitsSolverUsePosition->setChecked(Options::fitsSolverUsePosition());
    m_PlateSolveUI.kcfg_FitsSolverRadius->setValue(Options::fitsSolverRadius());

    m_PlateSolveUI.FitsSolverEstRA->setUnits(dmsBox::HOURS);
    m_PlateSolveUI.FitsSolverEstDec->setUnits(dmsBox::DEGREES);

    // Save the values of user controls when the user changes them.
    connect(m_PlateSolveUI.kcfg_FitsSolverProfile, QOverload<int>::of(&QComboBox::activated), [](int index)
    {
        Options::setFitsSolverProfile(index);
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
