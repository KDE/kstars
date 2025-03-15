/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imageoverlaycomponent.h"

#include "kstars.h"
#include "Options.h"
#include "skypainter.h"
#include "skymap.h"
#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsdata.h"
#endif
#include "auxiliary/kspaths.h"


#include <QTableWidget>
#include <QImageReader>
#include <QCheckBox>
#include <QComboBox>
#include <QtConcurrent>
#include <QRegularExpression>

#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"

namespace
{

enum ColumnIndex
{
    FILENAME_COL = 0,
    //ENABLED_COL,
    //NICKNAME_COL,
    STATUS_COL,
    RA_COL,
    DEC_COL,
    ARCSEC_PER_PIXEL_COL,
    ORIENTATION_COL,
    WIDTH_COL,
    HEIGHT_COL,
    EAST_TO_RIGHT_COL,
    NUM_COLUMNS
};

// These needs to be syncronized with enum Status and initializeGui::StatusNames().
constexpr int UNPROCESSED_INDEX = 0;
constexpr int OK_INDEX = 4;

// Helper to create the image overlay table.
// Start the table, displaying the heading and timing information, common to all sessions.
void setupTable(QTableWidget *table)
{
    table->clear();
    table->setRowCount(0);
    table->setColumnCount(NUM_COLUMNS);
    table->setShowGrid(false);
    table->setWordWrap(true);

    QStringList HeaderNames =
    {
        i18n("Filename"),
        //    "", "Nickname",
        i18n("Status"), i18n("RA"), i18n("DEC"), i18n("A-S/px"), i18n("Angle"),
        i18n("Width"), i18n("Height"), i18n("EastRight")
    };
    table->setHorizontalHeaderLabels(HeaderNames);
}

// This initializes an item in the table widget.
void setupTableItem(QTableWidget *table, int row, int column, const QString &text, bool editable = true)
{
    if (table->rowCount() < row + 1)
        table->setRowCount(row + 1);
    if (column >= NUM_COLUMNS)
        return;
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item->setText(text);
    if (!editable)
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
    table->setItem(row, column, item);
}

// Helper for sorting the overlays alphabetically (case-insensitive).
bool overlaySorter(const ImageOverlay &o1, const ImageOverlay &o2)
{
    return o1.m_Filename.toLower() < o2.m_Filename.toLower();
}

// The dms method for initializing from text requires knowing if the input is degrees or hours.
// This is a crude way to detect HMS input, and may not internationalize well.
bool isHMS(const QString &input)
{
    QString trimmedInput = input.trimmed();
    // Just 14h
    QRegularExpression re1("^(\\d+)\\s*h$");
    // 14h 2m
    QRegularExpression re2("^(\\d+)\\s*h\\s(\\d+)\\s*(?:[m\']?)$");
    // 14h 2m 3.2s
    QRegularExpression re3("^(\\d+)\\s*h\\s(\\d+)\\s*[m\'\\s]\\s*(\\d+\\.*\\d*)\\s*([s\"]?)$");

    return re1.match(trimmedInput).hasMatch() ||
           re2.match(trimmedInput).hasMatch() ||
           re3.match(trimmedInput).hasMatch();
}

// Even if an image is already solved, the user may have changed the status in the UI.
bool shouldSolveAnyway(QTableWidget *table, int row)
{
    QComboBox *item = dynamic_cast<QComboBox*>(table->cellWidget(row, STATUS_COL));
    if (!item) return false;
    return (item->currentIndex() != OK_INDEX);
}

QString toDecString(const dms &dec)
{
    // Sadly DMS::setFromString doesn't parse dms::toDMSString()
    // return dec.toDMSString();
    return QString("%1 %2' %3\"").arg(dec.degree()).arg(dec.arcmin()).arg(dec.arcsec());
}

QString toDecString(double dec)
{
    return toDecString(dms(dec));
}
}  // namespace

ImageOverlayComponent::ImageOverlayComponent(SkyComposite *parent) : SkyComponent(parent)
{
    QDir dir = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/imageOverlays");
    dir.mkpath(".");
    m_Directory = dir.absolutePath();
    connect(&m_TryAgainTimer, &QTimer::timeout, this, &ImageOverlayComponent::tryAgain, Qt::UniqueConnection);
    connect(this, &ImageOverlayComponent::updateLog, this, &ImageOverlayComponent::updateStatusDisplay, Qt::UniqueConnection);

    // Get the latest from the User DB
    loadFromUserDB();

    // The rest of the initialization happens when we get the setWidgets() call.
}

// Validate the user inputs, and if invalid, replace with the previous values.
void ImageOverlayComponent::cellChanged(int row, int col)
{
    if (!m_Initialized || col < 0 || col >= NUM_COLUMNS || row < 0 || row >= m_ImageOverlayTable->rowCount()) return;
    // Note there are a couple columns with widgets instead of normal text items.
    // This method shouldn't get called for those, but...
    if (col == STATUS_COL || col == EAST_TO_RIGHT_COL) return;

    QTableWidgetItem *item = m_ImageOverlayTable->item(row, col);
    if (!item) return;

    disconnect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged);
    QString itemString = item->text();
    auto overlay = m_Overlays[row];
    if (col == RA_COL)
    {
        dms raDMS;
        const bool useHMS = isHMS(itemString);
        const bool raOK = raDMS.setFromString(itemString, !useHMS);
        if (!raOK)
        {
            item->setText(dms(overlay.m_RA).toHMSString());
            QString msg = i18n("Bad RA string entered for %1. Reset to original value.", overlay.m_Filename);
            emit updateLog(msg);
        }
        else
            // Re-format the user-entered value.
            item->setText(raDMS.toHMSString());
    }
    else if (col == DEC_COL)
    {
        dms decDMS;
        const bool decOK = decDMS.setFromString(itemString);
        if (!decOK)
        {
            item->setText(toDecString(overlay.m_DEC));
            QString msg = i18n("Bad DEC string entered for %1. Reset to original value.", overlay.m_Filename);
            emit updateLog(msg);
        }
        else
            item->setText(toDecString(decDMS));
    }
    else if (col == ORIENTATION_COL)
    {
        bool angleOK = false;
        double angle = itemString.toDouble(&angleOK);
        if (!angleOK || angle > 360 || angle < -360)
        {
            item->setText(QString("%1").arg(overlay.m_Orientation, 0, 'f', 2));
            QString msg = i18n("Bad orientation angle string entered for %1. Reset to original value.", overlay.m_Filename);
            emit updateLog(msg);
        }
    }
    else if (col == ARCSEC_PER_PIXEL_COL)
    {
        bool scaleOK = false;
        double scale = itemString.toDouble(&scaleOK);
        if (!scaleOK || scale < 0 || scale > 1000)
        {
            item->setText(QString("%1").arg(overlay.m_ArcsecPerPixel, 0, 'f', 2));
            QString msg = i18n("Bad scale angle string entered for %1. Reset to original value.", overlay.m_Filename);
            emit updateLog(msg);
        }
    }
    connect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged, Qt::UniqueConnection);
}

// Like cellChanged() but for the status column which contains QComboxBox widgets.
void ImageOverlayComponent::statusCellChanged(int row)
{
    if (row < 0 || row >= m_ImageOverlayTable->rowCount()) return;

    auto overlay = m_Overlays[row];
    disconnect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged);

    // If the user changed the status of a column to OK,
    // then we check to make sure the required columns are filled out.
    // If so, then we also save it to the DB.
    // If the required columns are not filled out, the QComboBox value is reverted to UNPROCESSED.
    QComboBox *statusItem = dynamic_cast<QComboBox*>(m_ImageOverlayTable->cellWidget(row, STATUS_COL));
    bool failed = false;
    if (statusItem->currentIndex() == OK_INDEX)
    {
        dms raDMS;
        QTableWidgetItem *raItem = m_ImageOverlayTable->item(row, RA_COL);
        if (!raItem) return;
        const bool useHMS = isHMS(raItem->text());
        const bool raOK = raDMS.setFromString(raItem->text(), !useHMS);
        if (!raOK || raDMS.Degrees() == 0)
        {
            QString msg = i18n("Cannot set status to OK. Legal non-0 RA value required.");
            emit updateLog(msg);
            failed = true;
        }

        dms decDMS;
        QTableWidgetItem *decItem = m_ImageOverlayTable->item(row, DEC_COL);
        if (!decItem) return;
        const bool decOK = decDMS.setFromString(decItem->text());
        if (!decOK)
        {
            QString msg = i18n("Cannot set status to OK. Legal non-0 DEC value required.");
            emit updateLog(msg);
            failed = true;
        }

        bool angleOK = false;
        QTableWidgetItem *angleItem = m_ImageOverlayTable->item(row, ORIENTATION_COL);
        if (!angleItem) return;
        const double angle = angleItem->text().toDouble(&angleOK);
        if (!angleOK || angle > 360 || angle < -360)
        {
            QString msg = i18n("Cannot set status to OK. Legal orientation value required.");
            emit updateLog(msg);
            failed = true;
        }

        bool scaleOK = false;
        QTableWidgetItem *scaleItem = m_ImageOverlayTable->item(row, ARCSEC_PER_PIXEL_COL);
        if (!scaleItem) return;
        const double scale = scaleItem->text().toDouble(&scaleOK);
        if (!scaleOK || scale < 0 || scale > 1000)
        {
            QString msg = i18n("Cannot set status to OK. Legal non-0 a-s/px value required.");
            emit updateLog(msg);
            failed = true;
        }

        if (failed)
        {
            QComboBox *statusItem = dynamic_cast<QComboBox*>(m_ImageOverlayTable->cellWidget(row, STATUS_COL));
            statusItem->setCurrentIndex(UNPROCESSED_INDEX);
        }
        else
        {
            m_Overlays[row].m_Status = ImageOverlay::AVAILABLE;
            m_Overlays[row].m_RA = raDMS.Degrees();
            m_Overlays[row].m_DEC = decDMS.Degrees();
            m_Overlays[row].m_ArcsecPerPixel = scale;
            m_Overlays[row].m_Orientation = angle;
            const QComboBox *ewItem = dynamic_cast<QComboBox*>(m_ImageOverlayTable->cellWidget(row, EAST_TO_RIGHT_COL));
            m_Overlays[row].m_EastToTheRight = ewItem->currentIndex();

            if (m_Overlays[row].m_Img.get() == nullptr)
            {
                // Load the image.
                const QString fullFilename = QString("%1/%2").arg(m_Directory).arg(m_Overlays[row].m_Filename);
                QImage *img = loadImageFile(fullFilename, !m_Overlays[row].m_EastToTheRight);
                m_Overlays[row].m_Width = img->width();
                m_Overlays[row].m_Height = img->height();
                m_Overlays[row].m_Img.reset(img);
            }
            saveToUserDB();
            QString msg = i18n("Stored OK status for %1.", m_Overlays[row].m_Filename);
            emit updateLog(msg);
        }
    }
    connect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged, Qt::UniqueConnection);
}

ImageOverlayComponent::~ImageOverlayComponent()
{
    if (m_LoadImagesFuture.isRunning())
    {
        m_LoadImagesFuture.cancel();
        m_LoadImagesFuture.waitForFinished();
    }
}

void ImageOverlayComponent::selectionChanged()
{
    if (m_Initialized && Options::showSelectedImageOverlay())
        show();
}

bool ImageOverlayComponent::selected()
{
    return Options::showImageOverlays();
}

void ImageOverlayComponent::draw(SkyPainter *skyp)
{
#if !defined(KSTARS_LITE)
    if (m_Initialized)
    {
        skyp->drawImageOverlay(&m_Overlays);
        skyp->drawImageOverlay(&m_TemporaryOverlays);
    }
#else
    Q_UNUSED(skyp);
#endif
}

void ImageOverlayComponent::setWidgets(QTableWidget *table, QPlainTextEdit *statusDisplay,
                                       QPushButton *solveButton, QGroupBox *tableGroupBox,
                                       QComboBox *solverProfile)
{
    m_ImageOverlayTable = table;
    // Temporarily make the table uneditable.
    m_EditTriggers = m_ImageOverlayTable->editTriggers();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_SolveButton = solveButton;
    m_TableGroupBox = tableGroupBox;
    m_SolverProfile = solverProfile;
    solveButton->setText(i18n("Solve"));

    m_StatusDisplay = statusDisplay;
    setupTable(table);
    updateTable();
    connect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged, Qt::UniqueConnection);
    connect(m_ImageOverlayTable, &QTableWidget::itemSelectionChanged, this, &ImageOverlayComponent::selectionChanged,
            Qt::UniqueConnection);

    initSolverProfiles();
    loadAllImageFiles();
}

void ImageOverlayComponent::initSolverProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                            QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");

    QList<SSolver::Parameters> optionsList;
    if(QFile(savedOptionsProfiles).exists())
        optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        optionsList = Ekos::getDefaultAlignOptionsProfiles();

    m_SolverProfile->clear();
    for(auto &param : optionsList)
        m_SolverProfile->addItem(param.listName);
    m_SolverProfile->setCurrentIndex(Options::solveOptionsProfile());
}

void ImageOverlayComponent::updateStatusDisplay(const QString &message)
{
    if (!m_StatusDisplay)
        return;
    m_LogText.insert(0, message);
    m_StatusDisplay->setPlainText(m_LogText.join("\n"));
}

// Find all the files in the directory, see if they are in the m_Overlays.
// If no, append to the end of m_Overlays, and set status as unprocessed.
void ImageOverlayComponent::updateTable()
{
#ifdef HAVE_CFITSIO
    // Get the list of files from the image overlay directory.
    QDir directory(m_Directory);
    emit updateLog(i18n("Updating from directory: %1", m_Directory));
    QStringList images = directory.entryList(QStringList() << "*", QDir::Files);
    QSet<QString> imageFiles;
    foreach(QString filename, images)
    {
        if (!FITSData::readableFilename(filename))
            continue;
        imageFiles.insert(filename);
    }

    // Sort the files alphabetically.
    QList<QString> sortedImageFiles;
    for (const auto &fn : imageFiles)
        sortedImageFiles.push_back(fn);
    std::sort(sortedImageFiles.begin(), sortedImageFiles.end(), overlaySorter);

    // Remove any overlays that aren't in the directory.
    QList<ImageOverlay> tempOverlays;
    QMap<QString, int> tempMap;
    int numDeleted = 0;
    for (int i = 0; i < m_Overlays.size(); ++i)
    {
        auto &fname = m_Overlays[i].m_Filename;
        if (sortedImageFiles.indexOf(fname) >= 0)
        {
            tempOverlays.append(m_Overlays[i]);
            tempMap[fname] = tempOverlays.size() - 1;
        }
        else
            numDeleted++;
    }
    m_Overlays = tempOverlays;
    m_Filenames = tempMap;

    // Add the new files into the overlay list.
    int numNew = 0;
    for (const auto &filename : sortedImageFiles)
    {
        auto item = m_Filenames.find(filename);
        if (item == m_Filenames.end())
        {
            // If it doesn't already exist in our database:
            ImageOverlay overlay(filename);
            const int size = m_Filenames.size();  // place before push_back().
            m_Overlays.push_back(overlay);
            m_Filenames[filename] = size;
            numNew++;
        }
    }
    emit updateLog(i18n("%1 overlays (%2 new, %3 deleted) %4 solved", m_Overlays.size(), numNew, numDeleted,
                        numAvailable()));
    m_TableGroupBox->setTitle(i18n("Image Overlays.  %1 images, %2 available.", m_Overlays.size(), numAvailable()));

    initializeGui();
    saveToUserDB();
#endif
}

void ImageOverlayComponent::loadAllImageFiles()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_LoadImagesFuture = QtConcurrent::run(&ImageOverlayComponent::loadImageFileLoop, this);
#else
    m_LoadImagesFuture = QtConcurrent::run(this, &ImageOverlayComponent::loadImageFileLoop);
#endif
}

void ImageOverlayComponent::loadImageFileLoop()
{
    emit updateLog(i18n("Loading image files..."));
    while (loadImageFile());
    int num = 0;
    for (const auto &o : m_Overlays)
        if (o.m_Img.get() != nullptr)
            num++;
    emit updateLog(i18n("%1 image files loaded.", num));
    // Restore editing for the table.
    m_ImageOverlayTable->setEditTriggers(m_EditTriggers);
    m_Initialized = true;
}

void ImageOverlayComponent::addTemporaryImageOverlay(const ImageOverlay &overlay)
{
    m_TemporaryOverlays.push_back(overlay);
}

QImage *ImageOverlayComponent::loadImageFile (const QString &fullFilename, bool mirror)
{
    QSharedPointer<QImage> tempImage(new QImage(fullFilename));
    if (tempImage.get() == nullptr) return nullptr;
    int scaleWidth = std::min(tempImage->width(), Options::imageOverlayMaxDimension());
    QImage *processedImg = new QImage;
    if (mirror)
        *processedImg = tempImage->mirrored(true, false).scaledToWidth(scaleWidth); // It's reflected horizontally.
    else
        *processedImg = tempImage->scaledToWidth(scaleWidth);

    return processedImg;
}

bool ImageOverlayComponent::loadImageFile()
{
    bool updatedSomething = false;

    for (auto &o : m_Overlays)
    {
        if (o.m_Status == o.ImageOverlay::AVAILABLE && o.m_Img.get() == nullptr)
        {
            QString fullFilename = QString("%1%2%3").arg(m_Directory).arg(QDir::separator()).arg(o.m_Filename);
            QImage *img = loadImageFile(fullFilename, !o.m_EastToTheRight);
            o.m_Img.reset(img);
            updatedSomething = true;

            // Note: The original width and height in o.m_Width/m_Height is kept even
            // though the image was rescaled. This is to get the rendering right
            // with the original scale.
        }
    }
    return updatedSomething;
}


// Copies the info in m_Overlays into m_ImageOverlayTable UI.
void ImageOverlayComponent::initializeGui()
{
    if (!m_ImageOverlayTable) return;

    // Don't call callback on programmatic changes.
    disconnect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged);

    // This clears the table.
    setupTable(m_ImageOverlayTable);

    int row = 0;
    for (int i = 0; i < m_Overlays.size(); ++i)
    {
        const ImageOverlay &overlay = m_Overlays[row];
        // False: The user can't edit filename.
        setupTableItem(m_ImageOverlayTable, row, FILENAME_COL, overlay.m_Filename, false);

        QStringList StatusNames =
        {
            i18n("Unprocessed"), i18n("Bad File"), i18n("Solve Failed"), i18n("Error"), i18n("OK")
        };
        QComboBox *statusBox = new QComboBox();
        for (int i = 0; i < ImageOverlay::NUM_STATUS; ++i)
            statusBox->addItem(StatusNames[i]);
        connect(statusBox, QOverload<int>::of(&QComboBox::activated), this, [row, this](int newIndex)
        {
            Q_UNUSED(newIndex);
            statusCellChanged(row);
        });
        statusBox->setCurrentIndex(static_cast<int>(overlay.m_Status));
        m_ImageOverlayTable->setCellWidget(row, STATUS_COL, statusBox);

        setupTableItem(m_ImageOverlayTable, row, ORIENTATION_COL, QString("%1").arg(overlay.m_Orientation, 0, 'f', 2));
        setupTableItem(m_ImageOverlayTable, row, RA_COL, dms(overlay.m_RA).toHMSString());
        setupTableItem(m_ImageOverlayTable, row, DEC_COL, toDecString(overlay.m_DEC));
        setupTableItem(m_ImageOverlayTable, row, ARCSEC_PER_PIXEL_COL, QString("%1").arg(overlay.m_ArcsecPerPixel, 0, 'f', 2));

        // The user can't edit width & height--taken from image.
        setupTableItem(m_ImageOverlayTable, row, WIDTH_COL, QString("%1").arg(overlay.m_Width), false);
        setupTableItem(m_ImageOverlayTable, row, HEIGHT_COL, QString("%1").arg(overlay.m_Height), false);

        QComboBox *mirroredBox = new QComboBox();
        mirroredBox->addItem(i18n("West-Right"));
        mirroredBox->addItem(i18n("East-Right"));
        connect(mirroredBox, QOverload<int>::of(&QComboBox::activated), this, [row](int newIndex)
        {
            Q_UNUSED(row);
            Q_UNUSED(newIndex);
            // Don't need to do anything. Will get picked up on change of status to OK.
        });
        mirroredBox->setCurrentIndex(overlay.m_EastToTheRight ? 1 : 0);
        m_ImageOverlayTable->setCellWidget(row, EAST_TO_RIGHT_COL, mirroredBox);

        row++;
    }
    m_ImageOverlayTable->resizeColumnsToContents();
    m_TableGroupBox->setTitle(i18n("Image Overlays.  %1 images, %2 available.", m_Overlays.size(), numAvailable()));
    connect(m_ImageOverlayTable, &QTableWidget::cellChanged, this, &ImageOverlayComponent::cellChanged, Qt::UniqueConnection);

}

void ImageOverlayComponent::loadFromUserDB()
{
    QList<ImageOverlay *> list;
    KStarsData::Instance()->userdb()->GetAllImageOverlays(&m_Overlays);
    // Alphabetize.
    std::sort(m_Overlays.begin(), m_Overlays.end(), overlaySorter);
    m_Filenames.clear();
    int index = 0;
    for (const auto &o : m_Overlays)
    {
        m_Filenames[o.m_Filename] = index;
        index++;
    }
}

void ImageOverlayComponent::saveToUserDB()
{
    KStarsData::Instance()->userdb()->DeleteAllImageOverlays();
    for (const ImageOverlay &metadata : m_Overlays)
        KStarsData::Instance()->userdb()->AddImageOverlay(metadata);
}

void ImageOverlayComponent::solveImage(const QString &filename)
{
    if (!m_Initialized) return;
    m_SolveButton->setText(i18n("Abort"));
    const int solverTimeout = Options::imageOverlayTimeout();

    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                            QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");
    auto profiles = (QFile(savedOptionsProfiles).exists()) ?
                    StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles) :
                    Ekos::getDefaultAlignOptionsProfiles();

    const int index = m_SolverProfile->currentIndex();
    auto parameters = index < profiles.size() ? profiles.at(index) : profiles.at(0);
    // Double search radius
    parameters.search_radius = parameters.search_radius * 2;

    m_Solver.reset(new SolverUtils(parameters, solverTimeout),  &QObject::deleteLater);
    connect(m_Solver.get(), &SolverUtils::done, this, &ImageOverlayComponent::solverDone, Qt::UniqueConnection);

    if (m_RowsToSolve.size() > 1)
        emit updateLog(i18n("Solving: %1. %2 in queue.", filename, m_RowsToSolve.size()));
    else
        emit updateLog(i18n("Solving: %1.", filename));

    // If the user added some RA/DEC/Scale values to the table, they will be used in the solve
    // (but aren't remembered in the DB unless the solve is successful).
    int row = m_RowsToSolve[0];
    QString raString = m_ImageOverlayTable->item(row, RA_COL)->text().toLatin1().data();
    QString decString = m_ImageOverlayTable->item(row, DEC_COL)->text().toLatin1().data();
    QString scaleString = m_ImageOverlayTable->item(row, ARCSEC_PER_PIXEL_COL)->text().toLatin1().data();

    dms raDMS, decDMS;
    const bool useHMS = isHMS(raString);
    const bool raOK = raDMS.setFromString(raString, !useHMS) && (raDMS.Degrees() != 0.00);
    const bool decOK = decDMS.setFromString(decString) && (decDMS.Degrees() != 0.00);
    bool scaleOK = false;
    double scale = scaleString.toDouble(&scaleOK);
    scaleOK = scaleOK && scale != 0.00;

    // Use the default scale if it is > 0 and scale was not specified in the UI table.
    if (!scaleOK && Options::imageOverlayDefaultScale() > 0.0001)
    {
        scale = Options::imageOverlayDefaultScale();
        scaleOK = true;
    }

    if (scaleOK)
    {
        auto lowScale = scale * 0.75;
        auto highScale = scale * 1.25;
        m_Solver->useScale(true, lowScale, highScale);
    }
    if (raOK && decOK)
        m_Solver->usePosition(true, raDMS.Degrees(), decDMS.Degrees());

    m_Solver->runSolver(filename);
}

void ImageOverlayComponent::tryAgain()
{
    m_TryAgainTimer.stop();
    if (!m_Initialized) return;
    if (m_RowsToSolve.size() > 0)
        startSolving();
}

int ImageOverlayComponent::numAvailable()
{
    int num = 0;
    for (const auto &o : m_Overlays)
        if (o.m_Status == ImageOverlay::AVAILABLE)
            num++;
    return num;
}

void ImageOverlayComponent::show()
{
    if (!m_Initialized || !m_ImageOverlayTable) return;
    auto selections = m_ImageOverlayTable->selectionModel();
    if (selections->hasSelection())
    {
        auto selectedIndexes = selections->selectedIndexes();
        const int row = selectedIndexes.at(0).row();
        if (m_Overlays.size() > row && row >= 0)
        {
            if (m_Overlays[row].m_Status != ImageOverlay::AVAILABLE)
            {
                emit updateLog(i18n("Can't show %1. Not plate solved.", m_Overlays[row].m_Filename));
                return;
            }
            if (m_Overlays[row].m_Img.get() == nullptr)
            {
                emit updateLog(i18n("Can't show %1. Image not loaded.", m_Overlays[row].m_Filename));
                return;
            }
            const double ra = m_Overlays[row].m_RA;
            const double dec = m_Overlays[row].m_DEC;

            // Convert the RA/DEC from j2000 to jNow.
            auto localTime = KStarsData::Instance()->geo()->UTtoLT(KStarsData::Instance()->clock()->utc());
            const dms raDms(ra), decDms(dec);
            SkyPoint coord(raDms, decDms);
            coord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

            // Is this the right way to move to the SkyMap?
            Options::setIsTracking(false);
            SkyMap::Instance()->setFocusObject(nullptr);
            SkyMap::Instance()->setFocusPoint(nullptr);
            SkyMap::Instance()->setFocus(dms(coord.ra()), dms(coord.dec()));

            // Zoom factor is in pixels per radian.
            double zoomFactor = (400 * 60.0 *  10800.0) / (m_Overlays[row].m_Width * m_Overlays[row].m_ArcsecPerPixel * dms::PI);
            SkyMap::Instance()->setZoomFactor(zoomFactor);

            SkyMap::Instance()->forceUpdate(true);
        }
    }
}

void ImageOverlayComponent::abortSolving()
{
    if (!m_Initialized) return;
    m_RowsToSolve.clear();
    if (m_Solver)
        m_Solver->abort();
    emit updateLog(i18n("Solving aborted."));
    m_SolveButton->setText(i18n("Solve"));
}

void ImageOverlayComponent::startSolving()
{
    if (!m_Initialized) return;
    if (m_SolveButton->text() == i18n("Abort"))
    {
        abortSolving();
        return;
    }
    if (m_Solver && m_Solver->isRunning())
    {
        m_Solver->abort();
        if (m_RowsToSolve.size() > 0)
            m_TryAgainTimer.start(2000);
        return;
    }

    if (m_RowsToSolve.size() == 0)
    {
        QSet<int> selectedRows;
        auto selections = m_ImageOverlayTable->selectionModel();
        if (selections->hasSelection())
        {
            // Need to de-dup, as selecting the whole row will select all the columns.
            auto selectedIndexes = selections->selectedIndexes();
            for (int i = 0; i < selectedIndexes.count(); ++i)
            {
                // Don't insert a row that's already solved.
                const int row = selectedIndexes.at(i).row();
                if ((m_Overlays[row].m_Status == ImageOverlay::AVAILABLE) &&
                        !shouldSolveAnyway(m_ImageOverlayTable, row))
                {
                    emit updateLog(i18n("Skipping already solved: %1.", m_Overlays[row].m_Filename));
                    continue;
                }
                selectedRows.insert(row);
            }
        }
        m_RowsToSolve.clear();
        for (int row : selectedRows)
            m_RowsToSolve.push_back(row);
    }

    if (m_RowsToSolve.size() > 0)
    {
        const int row = m_RowsToSolve[0];
        const QString filename =
            QString("%1/%2").arg(m_Directory).arg(m_Overlays[row].m_Filename);
        if ((m_Overlays[row].m_Status == ImageOverlay::AVAILABLE) &&
                !shouldSolveAnyway(m_ImageOverlayTable, row))
        {
            emit updateLog(i18n("%1 already solved. Skipping.", filename));
            m_RowsToSolve.removeFirst();
            if (m_RowsToSolve.size() > 0)
                startSolving();
            return;
        }

        auto img = new QImage(filename);
        m_Overlays[row].m_Width = img->width();
        m_Overlays[row].m_Height = img->height();
        solveImage(filename);
    }
}

void ImageOverlayComponent::reload()
{
    if (!m_Initialized) return;
    m_Initialized = false;
    emit updateLog(i18n("Reloading. Image overlays temporarily disabled."));
    updateTable();
    loadAllImageFiles();
}

void ImageOverlayComponent::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution,
                                       double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &ImageOverlayComponent::solverDone);
    m_SolveButton->setText(i18n("Solve"));
    if (m_RowsToSolve.size() == 0)
        return;

    const int solverRow = m_RowsToSolve[0];
    m_RowsToSolve.removeFirst();

    QComboBox *statusItem = dynamic_cast<QComboBox*>(m_ImageOverlayTable->cellWidget(solverRow, STATUS_COL));
    if (timedOut)
    {
        emit updateLog(i18n("Solver timed out in %1s", QString::number(elapsedSeconds, 'f', 1)));
        m_Overlays[solverRow].m_Status = ImageOverlay::PLATE_SOLVE_FAILURE;
        statusItem->setCurrentIndex(static_cast<int>(m_Overlays[solverRow].m_Status));
    }
    else if (!success)
    {
        emit updateLog(i18n("Solver failed in %1s", QString::number(elapsedSeconds, 'f', 1)));
        m_Overlays[solverRow].m_Status = ImageOverlay::PLATE_SOLVE_FAILURE;
        statusItem->setCurrentIndex(static_cast<int>(m_Overlays[solverRow].m_Status));
    }
    else
    {
        m_Overlays[solverRow].m_Orientation = solution.orientation;
        m_Overlays[solverRow].m_RA = solution.ra;
        m_Overlays[solverRow].m_DEC = solution.dec;
        m_Overlays[solverRow].m_ArcsecPerPixel = solution.pixscale;
        m_Overlays[solverRow].m_EastToTheRight = solution.parity;
        m_Overlays[solverRow].m_Status = ImageOverlay::AVAILABLE;

        QString msg = i18n("Solver success in %1s: RA %2 DEC %3 Scale %4 Angle %5",
                           QString::number(elapsedSeconds, 'f', 1),
                           QString::number(solution.ra, 'f', 2),
                           QString::number(solution.dec, 'f', 2),
                           QString::number(solution.pixscale, 'f', 2),
                           QString::number(solution.orientation, 'f', 2));
        emit updateLog(msg);

        // Store the new values in the table.
        auto overlay = m_Overlays[solverRow];
        m_ImageOverlayTable->item(solverRow, RA_COL)->setText(dms(overlay.m_RA).toHMSString());
        m_ImageOverlayTable->item(solverRow, DEC_COL)->setText(toDecString(overlay.m_DEC));
        m_ImageOverlayTable->item(solverRow, ARCSEC_PER_PIXEL_COL)->setText(
            QString("%1").arg(overlay.m_ArcsecPerPixel, 0, 'f', 2));
        m_ImageOverlayTable->item(solverRow, ORIENTATION_COL)->setText(QString("%1").arg(overlay.m_Orientation, 0, 'f', 2));
        QComboBox *ewItem = dynamic_cast<QComboBox*>(m_ImageOverlayTable->cellWidget(solverRow, EAST_TO_RIGHT_COL));
        ewItem->setCurrentIndex(overlay.m_EastToTheRight ? 1 : 0);

        QComboBox *statusItem = dynamic_cast<QComboBox*>(m_ImageOverlayTable->cellWidget(solverRow, STATUS_COL));
        statusItem->setCurrentIndex(static_cast<int>(overlay.m_Status));

        // Load the image.
        QString fullFilename = QString("%1/%2").arg(m_Directory).arg(m_Overlays[solverRow].m_Filename);
        QImage *img = loadImageFile(fullFilename, !m_Overlays[solverRow].m_EastToTheRight);
        m_Overlays[solverRow].m_Img.reset(img);
    }
    saveToUserDB();

    if (m_RowsToSolve.size() > 0)
        startSolving();
    else
    {
        emit updateLog(i18n("Done solving. %1 available.", numAvailable()));
        m_TableGroupBox->setTitle(i18n("Image Overlays.  %1 images, %2 available.", m_Overlays.size(), numAvailable()));
    }
}
