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
#include "ksnotification.h"
#include "kstars.h"
#include "Options.h"
#include "ui_fitsheaderdialog.h"
#include "ui_statform.h"

#include <KMessageBox>
#include <QtConcurrent>
#include <QIcon>

#include <fits_debug.h>

namespace
{
const char kAutoToolTip[] = "Automatically find stretch parameters";
const char kStretchOffToolTip[] = "Stretch the image";
const char kStretchOnToolTip[] = "Disable stretching of the image.";
}  // namespace

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
    loadFITS(QUrl::fromLocalFile(recentImages->item(i)->text()));
}

void FITSTab::clearRecentFITS()
{
    disconnect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
    recentImages->clear();
    connect(recentImages, &QListWidget::currentRowChanged, this, &FITSTab::selectRecentFITS);
}

namespace
{

// Sets the text value in the slider's value display, and if adjustSlider is true,
// moves the slider to the correct position.
void setSlider(QSlider *slider, QLabel *label, float value, float maxValue, bool adjustSlider)
{
    if (adjustSlider)
        slider->setValue(static_cast<int>(value * 10000 / maxValue));
    QString valStr = QString("%1").arg(static_cast<double>(value), 5, 'f', 4);
    label->setText(valStr);
}

// Adds the following to a horizontal layout (left to right): a vertical line,
// a label with the slider's name, a slider, and a text field to display the slider's value.
void setupStretchSlider(QSlider *slider, QLabel *label, QLabel *val, int fontSize,
                        const QString &name, QHBoxLayout *layout)
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
    QFont font = label->font();
    font.setPointSize(fontSize);

    label->setText(name);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    label->setFont(font);
    layout->addWidget(label);
    slider->setMinimum(0);
    slider->setMaximum(10000);
    slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(slider);
    val->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    val->setFont(font);
    layout->addWidget(val);
}

// Adds a button with the icon and tooltip to the layout.
void setupStretchButton(QPushButton *button, const QString &iconName, const QString &tip, QHBoxLayout *layout)
{
    button->setIcon(QIcon::fromTheme(iconName));
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tip);
    button->setCheckable(true);
    button->setChecked(true);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(button);
}

}  // namespace

// Updates all the widgets in the stretch area to display the view's stretch parameters.
void FITSTab::setStretchUIValues(bool adjustSliders)
{
    StretchParams1Channel params = view->getStretchParams().grey_red;
    setSlider(shadowsSlider.get(), shadowsVal.get(), params.shadows, maxShadows, adjustSliders);
    setSlider(midtonesSlider.get(), midtonesVal.get(), params.midtones, maxMidtones, adjustSliders);
    setSlider(highlightsSlider.get(), highlightsVal.get(), params.highlights, maxHighlights, adjustSliders);


    bool stretchActive = view->isImageStretched();
    if (stretchActive)
    {
        stretchButton->setChecked(true);
        stretchButton->setToolTip(kStretchOnToolTip);
    }
    else
    {
        stretchButton->setChecked(false);
        stretchButton->setToolTip(kStretchOffToolTip);
    }

    // Only activate the auto button if stretching is on and auto-stretching is not set.
    if (stretchActive && !view->getAutoStretch())
    {
        autoButton->setEnabled(true);
        autoButton->setIcon(QIcon::fromTheme("tools-wizard"));
        autoButton->setIconSize(QSize(22, 22));
        autoButton->setToolTip(kAutoToolTip);
    }
    else
    {
        autoButton->setEnabled(false);
        autoButton->setIcon(QIcon());
        autoButton->setIconSize(QSize(22, 22));
        autoButton->setToolTip("");
    }
    autoButton->setChecked(view->getAutoStretch());

    // Disable most of the UI if stretching is not active.
    shadowsSlider->setEnabled(stretchActive);
    shadowsVal->setEnabled(stretchActive);
    shadowsLabel->setEnabled(stretchActive);
    midtonesSlider->setEnabled(stretchActive);
    midtonesVal->setEnabled(stretchActive);
    midtonesLabel->setEnabled(stretchActive);
    highlightsSlider->setEnabled(stretchActive);
    highlightsVal->setEnabled(stretchActive);
    highlightsLabel->setEnabled(stretchActive);
}

// Adjusts the maxShadows value so that we have room to adjust the slider.
void FITSTab::rescaleShadows()
{
    if (!view) return;
    StretchParams1Channel params = view->getStretchParams().grey_red;
    maxShadows = std::max(0.002f, std::min(1.0f, params.shadows * 2.0f));
    setStretchUIValues(true);
}

// Adjusts the maxMidtones value so that we have room to adjust the slider.
void FITSTab::rescaleMidtones()
{
    if (!view) return;
    StretchParams1Channel params = view->getStretchParams().grey_red;
    maxMidtones = std::max(.002f, std::min(1.0f, params.midtones * 2.0f));
    setStretchUIValues(true);
}

QHBoxLayout* FITSTab::setupStretchBar()
{
    constexpr int fontSize = 12;

    QHBoxLayout *stretchBarLayout = new QHBoxLayout();

    stretchButton.reset(new QPushButton());
    setupStretchButton(stretchButton.get(), "transform-move", kStretchOffToolTip, stretchBarLayout);

    // Shadows
    shadowsLabel.reset(new QLabel());
    shadowsVal.reset(new QLabel());
    shadowsSlider.reset(new QSlider(Qt::Horizontal, this));
    setupStretchSlider(shadowsSlider.get(), shadowsLabel.get(), shadowsVal.get(), fontSize, "Shadows", stretchBarLayout);

    // Midtones
    midtonesLabel.reset(new QLabel());
    midtonesVal.reset(new QLabel());
    midtonesSlider.reset(new QSlider(Qt::Horizontal, this));
    setupStretchSlider(midtonesSlider.get(), midtonesLabel.get(), midtonesVal.get(), fontSize, "Midtones", stretchBarLayout);

    // Highlights
    highlightsLabel.reset(new QLabel());
    highlightsVal.reset(new QLabel());
    highlightsSlider.reset(new QSlider(Qt::Horizontal, this));
    setupStretchSlider(highlightsSlider.get(), highlightsLabel.get(), highlightsVal.get(), fontSize, "Hightlights",
                       stretchBarLayout);

    // Separator
    QFrame* line4 = new QFrame();
    line4->setFrameShape(QFrame::VLine);
    line4->setFrameShadow(QFrame::Sunken);
    stretchBarLayout->addWidget(line4);

    autoButton.reset(new QPushButton());
    setupStretchButton(autoButton.get(), "tools-wizard", kAutoToolTip, stretchBarLayout);

    connect(stretchButton.get(), &QPushButton::clicked, [ = ]()
    {
        // This will toggle whether we're currently stretching.
        view->setStretch(!view->isImageStretched());
    });

    // Make rough displays for the slider movement.
    connect(shadowsSlider.get(), &QSlider::sliderMoved, [ = ](int value)
    {
        StretchParams params = view->getStretchParams();
        params.grey_red.shadows = this->maxShadows * value / 10000.0f;
        view->setSampling(4);
        view->setStretchParams(params);
        view->setSampling(1);
    });
    connect(midtonesSlider.get(), &QSlider::sliderMoved, [ = ](int value)
    {
        StretchParams params = view->getStretchParams();
        params.grey_red.midtones = this->maxMidtones * value / 10000.0f;
        view->setSampling(4);
        view->setStretchParams(params);
        view->setSampling(1);
    });
    connect(highlightsSlider.get(), &QSlider::sliderMoved, [ = ](int value)
    {
        StretchParams params = view->getStretchParams();
        params.grey_red.highlights = this->maxHighlights * value / 10000.0f;
        view->setSampling(4);
        view->setStretchParams(params);
        view->setSampling(1);
    });

    // Make a final full-res display when the slider is released.
    connect(shadowsSlider.get(), &QSlider::sliderReleased, [ = ]()
    {
        if (!view) return;
        rescaleShadows();
        StretchParams params = view->getStretchParams();
        view->setStretchParams(params);
    });
    connect(midtonesSlider.get(), &QSlider::sliderReleased, [ = ]()
    {
        if (!view) return;
        rescaleMidtones();
        StretchParams params = view->getStretchParams();
        view->setStretchParams(params);
    });
    connect(highlightsSlider.get(), &QSlider::sliderReleased, [ = ]()
    {
        if (!view) return;
        StretchParams params = view->getStretchParams();
        view->setStretchParams(params);
    });

    connect(autoButton.get(), &QPushButton::clicked, [ = ]()
    {
        // If we're not currently using automatic stretch parameters, turn that on.
        // If we're already using automatic parameters, don't do anything.
        // User can just move the sliders to take manual control.
        if (!view->getAutoStretch())
            view->setAutoStretchParams();
        else
            KMessageBox::information(this, "You are already using automatic stretching. To manually stretch, drag a slider.");
        setStretchUIValues(false);
    });

    // This is mostly useful right at the start, when the image is displayed without any user interaction.
    // Check for slider-in-use, as we don't wont to rescale while the user is active.
    connect(view.get(), &FITSView::newStatus, [ = ](const QString & ignored)
    {
        Q_UNUSED(ignored)
        bool slidersInUse = shadowsSlider->isSliderDown() || midtonesSlider->isSliderDown() ||
                            highlightsSlider->isSliderDown();
        if (!slidersInUse)
        {
            rescaleShadows();
            rescaleMidtones();
        }
        setStretchUIValues(!slidersInUse);
    });

    return stretchBarLayout;
}

bool FITSTab::setupView(FITSMode mode, FITSScale filter)
{
    if (view.get() == nullptr)
    {
        view.reset(new FITSView(this, mode, filter));
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *vlayout = new QVBoxLayout();

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
        vlayout->addLayout(setupStretchBar());

        connect(fitsSplitter, &QSplitter::splitterMoved, histogram, &FITSHistogram::resizePlot);

        setLayout(vlayout);
        connect(view.get(), &FITSView::newStatus, this, &FITSTab::newStatus);
        connect(view.get(), &FITSView::debayerToggled, this, &FITSTab::debayerToggled);

        // On Failure to load
        connect(view.get(), &FITSView::failed, this, &FITSTab::failed);

        return true;
    }

    // returns false if no setup needed.
    return false;
}

void FITSTab::loadFITS(const QUrl &imageURL, FITSMode mode, FITSScale filter, bool silent)
{
    if (setupView(mode, filter))
    {

        // On Success loading image
        connect(view.get(), &FITSView::loaded, [&]()
        {
            processData();
            emit loaded();
        });
    }

    currentURL = imageURL;

    view->setFilter(filter);

    view->loadFITS(imageURL.toLocalFile(), silent);
}

bool FITSTab::shouldComputeHFR() const
{
    if (viewer->isStarsMarked())
        return true;
    if (!Options::autoHFR())
        return false;
    return (view != nullptr) && (view->getMode() == FITS_NORMAL);
}

void FITSTab::processData()
{
    FITSData *image_data = view->getImageData();
    histogram->reset();
    image_data->setHistogram(histogram);

    // Only construct histogram if it is actually visible
    // Otherwise wait until histogram is needed before creating it.
    if (fitsSplitter->sizes().at(0) != 0)
    {
        histogram->constructHistogram();
    }

    if (shouldComputeHFR())
    {
        view->searchStars();
        qCDebug(KSTARS_FITS) << "FITS HFR:" << image_data->getHFR();
    }
    // This could both compute the HFRs and setup the graphics, however,
    // if shouldComputeHFR() above is true, then that will compute the HFRs
    // and this would notice that and just setup graphics. They are separated
    // for the case where the graphics is not desired.
    if (viewer->isStarsMarked())
        view->toggleStars(true);

    evaluateStats();

    loadFITSHeader();

    // Don't add it to the list if it is already there
    if (recentImages->findItems(currentURL.toLocalFile(), Qt::MatchExactly).count() == 0)
    {
        if(!image_data->isTempFile()) //Don't add it to the list if it is a preview
        {
            disconnect(recentImages, &QListWidget::currentRowChanged, this,
                       &FITSTab::selectRecentFITS);
            recentImages->addItem(currentURL.toLocalFile());
            recentImages->setCurrentRow(recentImages->count() - 1);
            connect(recentImages, &QListWidget::currentRowChanged,  this,
                    &FITSTab::selectRecentFITS);
        }
    }

    view->updateFrame();
}

bool FITSTab::loadFITSFromData(FITSData* data, const QUrl &imageURL,
                               FITSMode mode, FITSScale filter)
{
    setupView(mode, filter);

    currentURL = imageURL;

    view->setFilter(filter);

    if (!view->loadFITSFromData(data, imageURL.toLocalFile()))
    {
        // On Failure to load
        // connect(view.get(), &FITSView::failed, this, &FITSTab::failed);
        return false;
    }

    processData();
    return true;
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

bool FITSTab::saveImage(const QString &filename)
{
    return view->saveImage(filename);
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

    stat.statsTable->item(STAT_WIDTH, 0)->setText(QString::number(image_data->width()));
    stat.statsTable->item(STAT_HEIGHT, 0)->setText(QString::number(image_data->height()));
    stat.statsTable->item(STAT_BITPIX, 0)->setText(QString::number(image_data->bpp()));
    stat.statsTable->item(STAT_HFR, 0)->setText(QString::number(image_data->getHFR(), 'f', 3));

    if (image_data->channels() == 1)
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

    if (image_data->getMedian() == 0.0 && !histogram->isConstructed())
        histogram->constructHistogram();

    for (int i = 0; i < image_data->channels(); i++)
    {
        stat.statsTable->item(STAT_MIN, i)->setText(QString::number(image_data->getMin(i), 'f', 3));
        stat.statsTable->item(STAT_MAX, i)->setText(QString::number(image_data->getMax(i), 'f', 3));
        stat.statsTable->item(STAT_MEAN, i)->setText(QString::number(image_data->getMean(i), 'f', 3));
        stat.statsTable->item(STAT_MEDIAN, i)->setText(QString::number(image_data->getMedian(i), 'f', 3));
        stat.statsTable->item(STAT_STDDEV, i)->setText(QString::number(image_data->getStdDev(i), 'f', 3));
    }
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
            QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save FITS"), currentDir,
                                        "FITS (*.fits *.fits.gz *.fit);;Images (*.jpg *.png)");

        // if user presses cancel
        if (currentURL.isEmpty())
        {
            currentURL = backupCurrent;
            return false;
        }

        // If no extension is selected, assume FITS.
        if (currentURL.toLocalFile().contains('.') == 0)
            currentURL.setPath(currentURL.toLocalFile() + ".fits");
    }

    if (currentURL.isValid())
    {
        QString localFile = currentURL.toLocalFile();
        if (localFile.contains(".fit"))
            localFile = "!" + localFile;

        if (!saveImage(localFile))
        {
            KSNotification::error(i18n("Image save error: %1", view->getImageData()->getLastError()), i18n("Image Save"));
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
