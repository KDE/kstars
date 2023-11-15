/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aberrationinspector.h"
#include "aberrationinspectorplot.h"
#include "sensorgraphic.h"
#include <kstars_debug.h>
#include "kstars.h"
#include "Options.h"
#include <QSplitter>

const float RADIANS2DEGREES = 360.0f / (2.0f * M_PI);

namespace Ekos
{

AberrationInspector::AberrationInspector(const abInsData &data, const QVector<int> &positions,
        const QVector<QVector<double>> &measures, const QVector<QVector<double>> &weights,
        const QVector<QVector<int>> &numStars, const QVector<QPoint> &tileCenterOffsets) :
    m_data(data), m_positions(positions), m_measures(measures), m_weights(weights),
    m_numStars(numStars), m_tileOffsets(tileCenterOffsets)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    // 1. Setup the GUI
    setupUi(this);
    setupGUI();

    // 2. Initialise the widget
    initAberrationInspector();

    // 3. Curve fit the data for each tile and update Aberration Inspector with results
    fitCurves();

    // 4. Initialise the 3D graphic
    initGraphic();

    // 5. Restore persisted settings
    loadSettings();

    // 6. connect signals to persist changes to user settings
    connectSettings();

    // Display data appropriate to the tile selection
    setTileSelection(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

AberrationInspector::~AberrationInspector()
{
}

void AberrationInspector::setupGUI()
{
    // Set the title. Use run number to differentiate runs
    this->setWindowTitle(i18n("Aberration Inspector - Run %1", m_data.run));

    // Connect up button callbacks
    connect(aberrationInspectorButtonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, [this]()
    {
        this->done(QDialog::Accepted);
    });

    connect(abInsTileSelection, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        setTileSelection(static_cast<TileSelection>(index));
    });

    connect(abInsShowLabels, &QCheckBox::toggled, this, [&](bool setting)
    {
        setShowLabels(setting);
    });

    connect(abInsShowCFZ, &QCheckBox::toggled, this, [&](bool setting)
    {
        setShowCFZ(setting);
    });

    connect(abInsOptCentres, &QCheckBox::toggled, this, [&](bool setting)
    {
        setOptCentres(setting);
    });

    // Create a plot widget and add to the top of the dialog
    m_plot = new AberrationInspectorPlot(this);
    abInsPlotLayout->addWidget(m_plot);

    // Setup the results table
    QStringList Headers { i18n("Tile"), i18n("Description"), i18n("Solution"), i18n("Delta (ticks)"), i18n("Delta (μm)"), i18n("Num Stars"), i18n("R²"), i18n("Exclude")};
    abInsTable->setColumnCount(Headers.count());
    abInsTable->setHorizontalHeaderLabels(Headers);

    // Setup tooltips on column headers
    abInsTable->horizontalHeaderItem(0)->setToolTip(i18n("Tile"));
    abInsTable->horizontalHeaderItem(1)->setToolTip(i18n("Description"));
    abInsTable->horizontalHeaderItem(2)->setToolTip(i18n("Focuser Solution"));
    abInsTable->horizontalHeaderItem(3)->setToolTip(i18n("Delta from central tile in ticks"));
    abInsTable->horizontalHeaderItem(4)->setToolTip(i18n("Delta from central tile in micrometers"));
    abInsTable->horizontalHeaderItem(5)->setToolTip(i18n("Min / max number of stars detected in the focus run"));
    abInsTable->horizontalHeaderItem(6)->setToolTip(i18n("R²"));
    abInsTable->horizontalHeaderItem(7)->setToolTip(i18n("Check to exclude row from calculations"));

    // Prevent editing of table widget (except the exclude checkboxes) unless in production support mode
    QAbstractItemView::EditTrigger editTrigger = ABINS_DEBUG ? QAbstractItemView::DoubleClicked :
            QAbstractItemView::NoEditTriggers;
    abInsTable->setEditTriggers(editTrigger);

    // Connect up table widget events: cellEntered when mouse moves over a cell
    connect(abInsTable, &AbInsTableWidget::cellEntered, this, &AberrationInspector::newMousePos);
    // leaveTableEvent is signalled when the mouse is moved away from "table".
    connect(abInsTable, &AbInsTableWidget::leaveTableEvent, this, &AberrationInspector::leaveTableEvent);

    // Setup a SensorGraphic minimal window that acts like a visual tooltip
    sensorGraphic = new SensorGraphic(abInsTable, m_data.sensorWidth, m_data.sensorHeight, m_data.tileWidth);
}

// Mouse over table row, col. Show the sensor graphic
void AberrationInspector::newMousePos(int row, int column)
{
    if (column <= 1)
    {
        QTableWidgetItem *tile = abInsTable->item(row, 0);
        for (int i = 0; i < NUM_TILES; i++)
        {
            if (TILE_NAME[i] == tile->text())
            {
                // Set the highlight row
                sensorGraphic->setHighlight(i);
                // Move the graphic to the mouse position
                sensorGraphic->move(QCursor::pos());
                // If the graphic is hidden then show it; if details have changed then repaint it
                if (m_HighlightedRow != -1 && m_HighlightedRow != i)
                    sensorGraphic->repaint();
                else
                    sensorGraphic->show();
                m_HighlightedRow = row;
                return;
            }
        }
    }
    m_HighlightedRow = -1;
    sensorGraphic->hide();
}

// Called when table widget gets a mouse leave event; hide the sensor graphic
void AberrationInspector::leaveTableEvent()
{
    m_HighlightedRow = -1;
    if (sensorGraphic)
        sensorGraphic->hide();
}

// Called when the "Exclude" checkbox state is changed by the user
void AberrationInspector::onStateChanged(int state)
{
    Q_UNUSED(state);

    // Get the row and state of the checkbox
    QCheckBox* cb = qobject_cast<QCheckBox *>(QObject::sender());
    int row = cb->property("row").toInt();
    bool checked = cb->isChecked();

    // Set the exclude flag for the excluded tile
    setExcludeTile(row, checked, static_cast<TileSelection>(abInsTileSelection->currentIndex()));
    // Rerun the calcs so the newly changed exclude flag is taken into account
    setTileSelection(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

// Called when table cell has been changed
// This is a production support debug feature that is contolled by ABINS_DEBUG flag
// For normal use table editing is disabled and this function not called.
void AberrationInspector::onCellChanged(int row, int column)
{
    if (column != 3)
        return;

    // Get the new value
    QTableWidgetItem *item = abInsTable->item(row, column);
    int value = item->text().toInt();

    int tile = getTileFromRow(static_cast<TileSelection>(abInsTileSelection->currentIndex()), row);

    if (tile >= 0 && tile < NUM_TILES)
    {
        m_minimum[tile] = value + m_minimum[TILE_CM];
        // Rerun the calcs so the newly changed exclude flag is taken into account
        setTileSelection(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
    }
}

int AberrationInspector::getTileFromRow(TileSelection tileSelection, int row)
{
    int tile = -1;

    if (row < 0 || row >= NUM_TILES)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid row %2").arg(__FUNCTION__).arg(row);
        return tile;
    }

    switch(tileSelection)
    {
        case TileSelection::TILES_ALL:
            // Use all tiles
            tile = row;
            break;

        case TileSelection::TILES_OUTER_CORNERS:
            // Use tiles 0, 2, 4, 6, 8
            tile = row * 2;
            break;

        case TileSelection::TILES_INNER_DIAMOND:
            // Use tiles 1, 3, 4, 5, 7
            if (row < 2)
                tile = (row * 2) + 1;
            else if (row == 2)
                tile = 4;
            else
                tile = (row * 2) - 1;
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called but TileSelection invalid").arg(__FUNCTION__);
            break;
    }
    return tile;
}

void AberrationInspector::setExcludeTile(int row, bool checked, TileSelection tileSelection)
{
    if (row < 0 || row >= NUM_TILES)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid row %2").arg(__FUNCTION__).arg(row);
        return;
    }

    int tile = getTileFromRow(tileSelection, row);
    if (tile >= 0 && tile < NUM_TILES)
        m_excludeTile[tile] = checked;
}

void AberrationInspector::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::AberrationInspector::syncSettings);

    // All Checkboxes, except Sim mode checkbox - this should be defaulted to off when Aberratio Inspector starts
    for (auto &oneWidget : findChildren<QCheckBox*>())
        if (oneWidget != abInsSimMode)
            connect(oneWidget, &QCheckBox::toggled, this, &Ekos::AberrationInspector::syncSettings);

    // All Splitters
    for (auto &oneWidget : findChildren<QSplitter*>())
        connect(oneWidget, &QSplitter::splitterMoved, this, &Ekos::AberrationInspector::syncSettings);
}

void AberrationInspector::loadSettings()
{
    QString key;
    QVariant value;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
            oneWidget->setCurrentText(value.toString());
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "ComboBox Option" << key << "not found!";
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
    {
        key = oneWidget->objectName();
        if (key == abInsSimMode->objectName() || key == "")
            // Sim mode setting isn't persisted as it is always off on Aberration Inspector start.
            // Also the table widget has a column of checkboxes whose value is data dependent so these aren't persisted
            continue;

        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
            oneWidget->setChecked(value.toBool());
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Checkbox Option" << key << "not found!";
    }

    // All Splitters
    for (auto &oneWidget : findChildren<QSplitter*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            // Convert the saved QString to a QByteArray using Base64
            auto valueBA = QByteArray::fromBase64(value.toString().toUtf8());
            oneWidget->restoreState(valueBA);
        }
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Splitter Option" << key << "not found!";
    }
}

void AberrationInspector::syncSettings()
{
    QComboBox *cbox = nullptr;
    QCheckBox *cb = nullptr;
    QSplitter *s = nullptr;

    QString key;
    QVariant value;

    if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (s = qobject_cast<QSplitter*>(sender())))
    {
        key = s->objectName();
        // Convert from the QByteArray to QString using Base64
        value = QString::fromUtf8(s->saveState().toBase64());
    }

    // Save changed setting
    Options::self()->setProperty(key.toLatin1(), value);
    Options::self()->save();
}

// Setup display widgets to match tileSelection
void AberrationInspector::setTileSelection(TileSelection tileSelection)
{
    // Setup array of tiles to use, based on user selection
    setupTiles(tileSelection);
    // Redraw the v-curves based on user selection
    m_plot->redrawCurve(m_useTile);
    // Update the table widget based on user selection
    updateTable();
    // Update the results based on user selection
    analyseResults();
    // Update the 3D graphic based on user selection
    updateGraphic(tileSelection);
    // resize table widget based on contents
    tableResize();
    // Updates changes to the v-curves
    m_plot->replot();
}

void AberrationInspector::setupTiles(TileSelection tileSelection)
{
    switch(tileSelection)
    {
        case TileSelection::TILES_ALL:
            // Use all tiles
            for (int i = 0; i < NUM_TILES; i++)
                m_useTile[i] = true;
            break;

        case TileSelection::TILES_OUTER_CORNERS:
            // Use tiles TL, TR, CM, BL, BR
            m_useTile[TILE_TL] = m_useTile[TILE_TR] = m_useTile[TILE_CM] = m_useTile[TILE_BL] = m_useTile[TILE_BR] = true;
            m_useTile[TILE_TM] = m_useTile[TILE_CL] = m_useTile[TILE_CR] = m_useTile[TILE_BM] = false;
            break;

        case TileSelection::TILES_INNER_DIAMOND:
            // Use tiles TM, CL, CM, CR, BM
            m_useTile[TILE_TL] = m_useTile[TILE_TR] = m_useTile[TILE_BL] = m_useTile[TILE_BR] = false;
            m_useTile[TILE_TM] = m_useTile[TILE_CL] = m_useTile[TILE_CM] = m_useTile[TILE_CR] = m_useTile[TILE_BM] = true;
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid tile selection %2").arg(__FUNCTION__).arg(tileSelection);
            break;
    }
}

void AberrationInspector::setShowLabels(bool setting)
{
    m_plot->setShowLabels(setting);
    m_plot->replot();
}

void AberrationInspector::setShowCFZ(bool setting)
{
    m_plot->setShowCFZ(setting);
    m_plot->replot();
}

// Rerun calculations to take the Optimise Tile Centres setting into account
void AberrationInspector::setOptCentres(bool setting)
{
    Q_UNUSED(setting);
    setTileSelection(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

void AberrationInspector::initAberrationInspector()
{
    // Initialise the plot widget
    m_plot->init(m_data.yAxisLabel, m_data.starUnits, m_data.useWeights, abInsShowLabels->isChecked(),
                 abInsShowCFZ->isChecked());
}

// Resize the dialog to the data
void AberrationInspector::tableResize()
{
    // Resize the table columns to fit the data on display in it.
    abInsTable->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    abInsTable->verticalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

// Run curve fitting on the collected data for each tile, updating other widgets as we go
void AberrationInspector::fitCurves()
{
    curveFitting.reset(new CurveFitting());

    const double expected = 0.0;
    int minPos, maxPos;

    QVector<bool> outliers;
    for (int i = 0; i < m_positions.count(); i++)
    {
        outliers.append(false);
        if (i == 0)
            minPos = maxPos = m_positions[i];
        else
        {
            minPos = std::min(minPos, m_positions[i]);
            maxPos = std::max(maxPos, m_positions[i]);
        }
    }

    for (int tile = 0; tile < m_measures.count(); tile++)
    {
        curveFitting->fitCurve(CurveFitting::FittingGoal::BEST, m_positions, m_measures[tile], m_weights[tile], outliers,
                               m_data.curveFit, m_data.useWeights, m_data.optDir);

        double position = 0.0;
        double measure = 0.0;
        double R2 = 0.0;
        bool foundFit = curveFitting->findMinMax(expected, static_cast<double>(minPos), static_cast<double>(maxPos), &position,
                        &measure, m_data.curveFit, m_data.optDir);
        if (foundFit)
            R2 = curveFitting->calculateR2(m_data.curveFit);

        position = round(position);
        m_minimum.append(position);
        m_minMeasure.append(measure);
        m_fit.append(foundFit);
        m_R2.append(R2);

        // Add the datapoints to the plot for the current tile
        // JEE Need to sort out what to do with outliers... for now ignore them
        QVector<bool> outliers;
        for (int i = 0; i < m_measures[tile].count(); i++)
        {
            outliers.append(false);
        }

        m_plot->addData(m_positions, m_measures[tile], m_weights[tile], outliers);
        // Fit the curve - note this needs curveFitting with the parameters for the current solution
        m_plot->drawCurve(tile, curveFitting.get(), position, measure, foundFit, R2);
        // Draw solutions on the plot
        m_plot->drawMaxMin(tile, position, measure);
        // Draw the CFZ for the central tile
        if (tile == TILE_CM)
            m_plot->drawCFZ(position, measure, m_data.cfzSteps);
    }
}

// Update the results table
void AberrationInspector::updateTable()
{
    // Disconnect the cell changed callback to stop it firing whilst the table is updated
    disconnect(abInsTable, &AbInsTableWidget::cellChanged, this, &AberrationInspector::onCellChanged);

    if (abInsTileSelection->currentIndex() == TILES_ALL)
        abInsTable->setRowCount(NUM_TILES);
    else
        abInsTable->setRowCount(5);

    int rowCounter = -1;
    for (int i = 0; i < NUM_TILES; i++)
    {
        if (!m_useTile[i])
            continue;

        ++rowCounter;

        QTableWidgetItem *tile = new QTableWidgetItem(TILE_NAME[i]);
        tile->setForeground(QColor(TILE_COLOUR[i]));
        abInsTable->setItem(rowCounter, 0, tile);

        QTableWidgetItem *description = new QTableWidgetItem(TILE_LONGNAME[i]);
        abInsTable->setItem(rowCounter, 1, description);

        QTableWidgetItem *solution = new QTableWidgetItem(QString::number(m_minimum[i]));
        solution->setTextAlignment(Qt::AlignRight);
        abInsTable->setItem(rowCounter, 2, solution);

        int ticks = m_minimum[i] - m_minimum[TILE_CM];
        QTableWidgetItem *deltaTicks = new QTableWidgetItem(QString::number(ticks));
        deltaTicks->setTextAlignment(Qt::AlignRight);
        abInsTable->setItem(rowCounter, 3, deltaTicks);

        int microns = ticks * m_data.focuserStepMicrons;
        QTableWidgetItem *deltaMicrons = new QTableWidgetItem(QString::number(microns));
        deltaMicrons->setTextAlignment(Qt::AlignRight);
        abInsTable->setItem(rowCounter, 4, deltaMicrons);

        int minNumStars = *std::min_element(m_numStars[i].constBegin(), m_numStars[i].constEnd());
        int maxNumStars = *std::max_element(m_numStars[i].constBegin(), m_numStars[i].constEnd());
        QTableWidgetItem *numStars = new QTableWidgetItem(QString("%1 / %2").arg(minNumStars).arg(maxNumStars));
        numStars->setTextAlignment(Qt::AlignRight);
        abInsTable->setItem(rowCounter, 5, numStars);

        QTableWidgetItem *R2 = new QTableWidgetItem(QString("%1").arg(m_R2[i], 0, 'f', 2));
        R2->setTextAlignment(Qt::AlignRight);
        abInsTable->setItem(rowCounter, 6, R2);

        QWidget *checkBoxWidget = new QWidget(abInsTable);
        QCheckBox *checkBox = new QCheckBox();
        // Set the checkbox based on whether curve fitting worked
        checkBox->setChecked(!m_fit[i] || m_excludeTile[i]);
        checkBox->setEnabled(m_fit[i]);
        // Add a property to identify the row when the user changes the check state.
        checkBox->setProperty("row", rowCounter);
        // In order to centre the widget, we need to insert it into a layout and align that
        QHBoxLayout *layoutCheckBox = new QHBoxLayout(checkBoxWidget);
        layoutCheckBox->addWidget(checkBox);
        layoutCheckBox->setAlignment(Qt::AlignCenter);

        abInsTable->setCellWidget(rowCounter, 7, checkBoxWidget);
        // The tableWidget cellChanged event doesn't fire when the checkbox state is changed.
        // Seems like the only way to get the event is to connect up directly to the checkbox
        connect(checkBox, &QCheckBox::stateChanged, this, &AberrationInspector::onStateChanged);
    }
    connect(abInsTable, &AbInsTableWidget::cellChanged, this, &AberrationInspector::onCellChanged);

    // Update the sensor graphic with the current tile selection
    sensorGraphic->setTiles(m_useTile);
}

// Analyse the results for backfocus delta and tilt based on tile selection.
void AberrationInspector::analyseResults()
{
    // Backfocus
    // +result = move sensor nearer field flattener
    // -result = move sensor further from field flattener
    bool backfocusOK = calcBackfocusDelta(static_cast<TileSelection>(abInsTileSelection->currentIndex()), m_backfocus);

    // Update backfocus screen widgets
    if (!backfocusOK)
        backfocus->setText(i18n("N/A"));
    else
    {
        QString side = "";
        if (m_backfocus < -0.9)
            side = i18n("Move sensor nearer flattener");
        else if (m_backfocus > 0.9)
            side = i18n("Move sensor away from flattener");
        backfocus->setText(QString("%1μm. %2").arg(m_backfocus, 0, 'f', 0).arg(side));
    }

    // Tilt
    bool tiltOK = calcTilt();

    // Update tilt screen widgets
    if (!tiltOK)
    {
        lrTilt->setText(i18n("N/A"));
        tbTilt->setText(i18n("N/A"));
        totalTilt->setText(i18n("N/A"));
    }
    else
    {
        lrTilt->setText(QString("%1μm / %2%").arg(m_LRMicrons, 0, 'f', 0).arg(m_LRTilt, 0, 'f', 2));
        tbTilt->setText(QString("%1μm / %2%").arg(m_TBMicrons, 0, 'f', 0).arg(m_TBTilt, 0, 'f', 2));
        totalTilt->setText(QString("%1μm / %2%").arg(m_diagonalMicrons, 0, 'f', 0)
                           .arg(m_diagonalTilt, 0, 'f', 2));
    }

    // Set m_resultsOK dependent on whether calcs were successful or not
    m_resultsOK = backfocusOK && tiltOK;
}

// Calculate the backfocus in microns
// Note that the tile positions are at different distances from the sensor centre so we need to weight
// values by the distance to tile centre. Also, we only include tiles for which curve fitting worked
// and for which the user hasn't elected to exclude
bool AberrationInspector::calcBackfocusDelta(TileSelection tileSelection, double &backfocusDelta)
{
    backfocusDelta = 0.0;

    // Firstly check that we have a valid central tile - we can't do anything without that
    if (!m_fit[TILE_CM] || m_excludeTile[TILE_CM])
        return false;

    double dist = 0.0, sum = 0.0, counter = 0.0;

    switch(tileSelection)
    {
        case TileSelection::TILES_ALL:
            // Use all useable tiles weighted by their distance from the centre
            for (int i = 0; i < NUM_TILES; i++)
            {
                if (i == TILE_CM)
                    continue;

                if (m_fit[i] && !m_excludeTile[i])
                {
                    dist = getXYTileCentre(static_cast<tileID>(i)).length();
                    sum += m_minimum[i] * dist;
                    counter += dist;
                }
            }
            if (counter == 0)
                // No valid tiles so can't complete the calc
                return false;
            break;

        case TileSelection::TILES_OUTER_CORNERS:
            // All tiles are diagonal from centre... so no need to weight the calc
            // Use tiles 0, 2, 6, 8
            if (m_fit[0] && !m_excludeTile[0])
            {
                sum += m_minimum[0];
                counter++;
            }
            if (m_fit[2] && !m_excludeTile[2])
            {
                sum += m_minimum[2];
                counter++;
            }
            if (m_fit[6] && !m_excludeTile[6])
            {
                sum += m_minimum[6];
                counter++;
            }
            if (m_fit[8] && !m_excludeTile[8])
            {
                sum += m_minimum[8];
                counter++;
            }

            if (counter == 0)
                // No valid tiles so can't complete the calc
                return false;
            break;

        case TileSelection::TILES_INNER_DIAMOND:
            // Tiles are different distances from centre... so need to weight the calc
            // Use tiles 1, 3, 5, 7
            if (m_fit[TILE_TM] && !m_excludeTile[TILE_TM])
            {
                dist = getXYTileCentre(TILE_TM).length();
                sum += m_minimum[TILE_TM] * dist;
                counter += dist;
            }
            if (m_fit[TILE_CL] && !m_excludeTile[TILE_CL])
            {
                dist = getXYTileCentre(TILE_CL).length();
                sum += m_minimum[TILE_CL] * dist;
                counter += dist;
            }
            if (m_fit[TILE_CR] && !m_excludeTile[TILE_CR])
            {
                dist = getXYTileCentre(TILE_CR).length();
                sum += m_minimum[TILE_CR] * dist;
                counter += dist;
            }
            if (m_fit[TILE_BM] && !m_excludeTile[TILE_BM])
            {
                dist = getXYTileCentre(TILE_BM).length();
                sum += m_minimum[TILE_BM] * dist;
                counter += dist;
            }

            if (counter == 0)
                // No valid tiles so can't complete the calc
                return false;
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid tile selection %2").arg(__FUNCTION__).arg(tileSelection);
            return false;
            break;
    }
    backfocusDelta = (m_minimum[TILE_CM] - (sum / counter)) * m_data.focuserStepMicrons;
    return true;
}

bool AberrationInspector::calcTilt()
{
    // Firstly check that we have a valid central tile - we can't do anything without that
    if (!m_fit[TILE_CM] || m_excludeTile[TILE_CM])
        return false;

    // Calculate the deltas relative to the centre tile in microns
    m_deltas.clear();
    for (int tile = 0; tile < NUM_TILES; tile++)
        m_deltas.append((m_minimum[TILE_CM] - m_minimum[tile]) * m_data.focuserStepMicrons);

    // Calculate the average tile position for left, right, top and bottom. If any of these cannot be
    // calculated then fail the whole calculation.
    double avLeft, avRight, avTop, avBottom;
    int tilesL[3] = { 0, 3, 6 };
    if (!avTiles(tilesL, avLeft))
        return false;
    int tilesR[3] = { 2, 5, 8 };
    if (!avTiles(tilesR, avRight))
        return false;
    int tilesT[3] = { 0, 1, 2 };
    if (!avTiles(tilesT, avTop))
        return false;
    int tilesB[3] = { 6, 7, 8 };
    if(!avTiles(tilesB, avBottom))
        return false;

    m_LRMicrons = avLeft - avRight;
    m_TBMicrons = avTop - avBottom;
    m_diagonalMicrons = std::hypot(m_LRMicrons, m_TBMicrons);

    // Calculate the sensor spans in microns
    const double LRSpan = (m_data.sensorWidth - m_data.tileWidth) * m_data.pixelSize;
    const double TBSpan = (m_data.sensorHeight - m_data.tileWidth) * m_data.pixelSize;

    // Calculate the tilt as a % slope
    m_LRTilt = m_LRMicrons / LRSpan * 100.0;
    m_TBTilt = m_TBMicrons / TBSpan * 100.0;
    m_diagonalTilt = std::hypot(m_LRTilt, m_TBTilt);
    return true;
}

// Averages upto 3 passed in tile values.
bool AberrationInspector::avTiles(int tiles[3], double &average)
{
    double sum = 0.0;
    int counter = 0;
    for (int i = 0; i < 3; i++)
    {
        if (m_useTile[tiles[i]] && m_fit[tiles[i]] && !m_excludeTile[tiles[i]])
        {
            sum += m_deltas[tiles[i]];
            counter++;
        }
    }
    if (counter > 0)
        average = sum / counter;
    return (counter > 0);
}

// Initialise the 3D graphic
void AberrationInspector::initGraphic()
{
    // Create a 3D Surface widget and add to the dialog
    m_graphic = new Q3DSurface();
    QWidget *container = QWidget::createWindowContainer(m_graphic);

    // abInsHSplitter is created in the .ui file but, by default, doesn't work - don't know why
    // Workaround is to create a new QSplitter object and use that.
    abInsHSplitter = new QSplitter(abInsVSplitter);
    abInsHSplitter->setObjectName(QString::fromUtf8("abInsHSplitter"));
    abInsHSplitter->addWidget(tableAndResultsWidget);
    abInsHSplitter->addWidget(widget);
    hLayout->insertWidget(0, container, 1);
    auto value = Options::abInsHSplitter();
    // Convert the saved QString to a QByteArray using Base64
    auto valueBA = QByteArray::fromBase64(value.toUtf8());
    abInsHSplitter->restoreState(valueBA);
    connect(abInsHSplitter, &QSplitter::splitterMoved, this, &Ekos::AberrationInspector::syncSettings);

    connect(abInsSelection, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        if (index == 0)
            m_graphic->setSelectionMode(QAbstract3DGraph::SelectionNone);
        else if (index == 1)
            m_graphic->setSelectionMode(QAbstract3DGraph::SelectionItem);
        else if (index == 2)
            m_graphic->setSelectionMode(QAbstract3DGraph::SelectionItemAndColumn | QAbstract3DGraph::SelectionSlice |
                                        QAbstract3DGraph::SelectionMultiSeries);
    });

    connect(abInsTheme, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        m_graphic->activeTheme()->setType(Q3DTheme::Theme(index));
    });

    connect(abInsLabels, &QCheckBox::toggled, this, [&](bool setting)
    {
        m_graphicLabels = setting;
        updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
    });

    connect(abInsSensor, &QCheckBox::toggled, this, [&](bool setting)
    {
        m_graphicSensor = setting;
        updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
    });

    connect(abInsPetzvalWire, &QCheckBox::toggled, this, [&](bool setting)
    {
        m_graphicPetzvalWire = setting;
        updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
    });
    connect(abInsPetzvalSurface, &QCheckBox::toggled, this, [&](bool setting)
    {
        m_graphicPetzvalSurface = setting;
        updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
    });

    // Simulation on/off
    connect(abInsSimMode, &QCheckBox::toggled, this, &AberrationInspector::simModeToggled);

    m_sensor = new QSurface3DSeries;
    m_sensorProxy = new QSurfaceDataProxy();
    m_sensor->setDataProxy(m_sensorProxy);
    m_graphic->addSeries(m_sensor);

    m_petzval = new QSurface3DSeries;
    m_petzvalProxy = new QSurfaceDataProxy();
    m_petzval->setDataProxy(m_petzvalProxy);
    m_graphic->addSeries(m_petzval);

    m_graphic->axisX()->setTitle("X Axis (μm) - Sensor Left to Right");
    m_graphic->axisY()->setTitle("Y Axis (μm) - Sensor Top to Bottom");
    m_graphic->axisZ()->setTitle("Z Axis (μm) - Axis of Telescope");

    m_graphic->axisX()->setLabelFormat("%.0f");
    m_graphic->axisY()->setLabelFormat("%.0f");
    m_graphic->axisZ()->setLabelFormat("%.0f");

    m_graphic->axisX()->setTitleVisible(true);
    m_graphic->axisY()->setTitleVisible(true);
    m_graphic->axisZ()->setTitleVisible(true);

    m_graphic->activeTheme()->setType(Q3DTheme::ThemePrimaryColors);

    // Set projection and shadows
    m_graphic->setOrthoProjection(false);
    m_graphic->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
    // Balance the z-axis with the x-y plane. Without this the z-axis is crushed to a very small scale
    m_graphic->setHorizontalAspectRatio(2.0);
}

// Simulation on/off switch toggled
void AberrationInspector::simModeToggled(bool setting)
{
    m_simMode = setting;
    abInsBackfocusSlider->setEnabled(setting);
    abInsTiltLRSlider->setEnabled(setting);
    abInsTiltTBSlider->setEnabled(setting);
    if (!m_simMode)
    {
        // Reset the sliders
        abInsBackfocusSlider->setRange(0, 10);
        abInsBackfocusSlider->setValue(0);
        abInsTiltLRSlider->setRange(0, 10);
        abInsTiltLRSlider->setValue(0);
        abInsTiltTBSlider->setRange(0, 10);
        abInsTiltTBSlider->setValue(0);

        // Reset the curve fitting object in case Sim mode has caused any problems. This will ensure the graphic
        // can always return to its original state after using sim mode.
        curveFitting.reset(new CurveFitting());
    }
    else
    {
        // Disconnect slider signals which initialising sliders
        disconnect(abInsBackfocusSlider, &QSlider::valueChanged, this, &AberrationInspector::simBackfocusChanged);
        disconnect(abInsTiltLRSlider, &QSlider::valueChanged, this, &AberrationInspector::simLRTiltChanged);
        disconnect(abInsTiltTBSlider, &QSlider::valueChanged, this, &AberrationInspector::simTBTiltChanged);

        // Setup backfocus slider.
        int range = 10;
        abInsBackfocusSlider->setRange(-range, range);
        int sign = m_backfocus < 0.0 ? -1 : 1;
        abInsBackfocusSlider->setValue(sign * 5);
        abInsBackfocusSlider->setTickInterval(1);
        m_simBackfocus = m_backfocus;

        // Setup Left-to-Right tilt slider.
        abInsTiltLRSlider->setRange(-range, range);
        sign = m_LRTilt < 0.0 ? -1 : 1;
        abInsTiltLRSlider->setValue(sign * 5);
        abInsTiltLRSlider->setTickInterval(1);
        m_simLRTilt = m_LRTilt;

        // Setup Top-to-Bottom tilt slider
        abInsTiltTBSlider->setRange(-range, range);
        sign = m_TBTilt < 0.0 ? -1 : 1;
        abInsTiltTBSlider->setValue(sign * 5);
        abInsTiltTBSlider->setTickInterval(1);
        m_simTBTilt = m_TBTilt;

        // Now that the sliders have been initialised, connect up signals
        connect(abInsBackfocusSlider, &QSlider::valueChanged, this, &AberrationInspector::simBackfocusChanged);
        connect(abInsTiltLRSlider, &QSlider::valueChanged, this, &AberrationInspector::simLRTiltChanged);
        connect(abInsTiltTBSlider, &QSlider::valueChanged, this, &AberrationInspector::simTBTiltChanged);
    }
    updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

// Update the 3D graphic based on user selections
void AberrationInspector::updateGraphic(TileSelection tileSelection)
{
    // If we don't have good results then don't display the 3D graphic
    bool ok = m_resultsOK;

    if (ok)
        // Display thw sensor
        ok = processSensor();

    if (ok)
    {
        // Add labels to the sensor
        processSensorLabels();

        // Draw the Petzval surface (from the field flattener
        ok = processPetzval(tileSelection);
    }

    if (ok)
    {
        // Setup axes
        m_graphic->axisX()->setRange(-m_maxX, m_maxX);
        m_graphic->axisY()->setRange(-m_maxY, m_maxY);
        m_graphic->axisZ()->setRange(m_minZ * 1.1, m_maxZ * 1.1);

        // Display / don't display the sensor
        m_graphicSensor ? m_graphic->addSeries(m_sensor) : m_graphic->removeSeries(m_sensor);

        // Display Petzval curve
        QSurface3DSeries::DrawFlags petzvalDrawMode { 0 };
        if (m_graphicPetzvalWire)
            petzvalDrawMode = petzvalDrawMode | QSurface3DSeries::DrawWireframe;
        if (m_graphicPetzvalSurface)
            petzvalDrawMode = petzvalDrawMode | QSurface3DSeries::DrawSurface;

        if (petzvalDrawMode)
        {
            m_petzval->setDrawMode(petzvalDrawMode);
            m_graphic->addSeries(m_petzval);
        }
        else
            m_graphic->removeSeries(m_petzval);
    }

    // Display / don't display the graphic
    ok ? m_graphic->show() : m_graphic->hide();
}

// Draw the sensor on the graphic
bool AberrationInspector::processSensor()
{
    // If we are in Sim mode we have previously solved the equation for the plane. All movements in
    // Sim mode are rotations.
    // If we are not in Sim mode then resolve, e.g. when tile selection changes
    if (!m_simMode)
    {
        // Fit a plane to the datapoints for the selected tiles. Fit is unweighted
        CurveFitting::DataPoint3DT plane;
        plane.useWeights = false;
        for (int tile = 0; tile < NUM_TILES; tile++)
        {
            if (m_useTile[tile])
            {
                QVector3D tileCentre = QVector3D(getXYTileCentre(static_cast<Ekos::tileID>(tile)),
                                                 getBSDelta(static_cast<Ekos::tileID>(tile)));
                plane.push_back(tileCentre.x(), tileCentre.y(), tileCentre.z());
                // Update the graph range to accomodate the data
                m_maxX = std::max(m_maxX, tileCentre.x());
                m_maxY = std::max(m_maxY, tileCentre.y());
                m_maxZ = std::max(m_maxZ, tileCentre.z());
                m_minZ = std::min(m_minZ, tileCentre.z());
            }
        }
        curveFitting->fitCurve3D(plane, CurveFitting::FOCUS_PLANE);
        double R2 = curveFitting->calculateR2(CurveFitting::FOCUS_PLANE);
        // JEE need to think about how to handle failure to solve versus boundary condition of R2=0
        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 sensor plane solved R2=%2").arg(__FUNCTION__).arg(R2);
    }

    // We've successfully solved the plane of the sensor so load the sensor vertices in the 3D Surface
    // getSensorVertex will perform the necessary rotations
    QSurfaceDataArray *data = new QSurfaceDataArray;
    QSurfaceDataRow *dataRow1 = new QSurfaceDataRow;
    QSurfaceDataRow *dataRow2 = new QSurfaceDataRow;
    *dataRow1 << getSensorVertex(TILE_TL) << getSensorVertex(TILE_TR);
    *dataRow2 << getSensorVertex(TILE_BL) << getSensorVertex(TILE_BR);
    *data << dataRow1 << dataRow2;
    m_sensorProxy->resetArray(data);
    m_sensor->setDrawMode(QSurface3DSeries::DrawSurface);
    return true;
}

void AberrationInspector::processSensorLabels()
{
    // Now sort out the labels on the sensor
    for (int tile = 0; tile < NUM_TILES; tile++)
    {
        if (m_label[tile] == nullptr)
            m_label[tile] = new QCustom3DLabel();
        else
        {
            m_graphic->removeCustomItem(m_label[tile]);
            m_label[tile] = new QCustom3DLabel();
        }
        m_label[tile]->setText(TILE_NAME[tile]);
        m_label[tile]->setTextColor(TILE_COLOUR[tile]);
        QVector3D pos = getLabelCentre(static_cast<tileID>(tile));
        m_label[tile]->setPosition(pos);

        m_maxX = std::max(m_maxX, pos.x());
        m_maxY = std::max(m_maxY, pos.y());
        m_maxZ = std::max(m_maxZ, pos.z());
        m_minZ = std::min(m_minZ, pos.z());

        QFont font = m_label[tile]->font();
        font.setPointSize(500);
        m_label[tile]->setFont(font);
        m_label[tile]->setFacingCamera(true);
    }

    for (int tile = 0; tile < NUM_TILES; tile++)
    {
        if (m_useTile[tile] && m_graphicLabels)
            m_graphic->addCustomItem(m_label[tile]);
    }
}

// Draw the Petzval surface on the graphic. Assume a parabolid surface
// z = x^2/a^2 + y^2/b^2. Assume symmetry where a = b
// a^2 = (x^2 + y^2) / z
//
// We know that at the measured datapoints (tile centres) the z value = backfocus
// This is complicated by sensor tilt, but the previously calculated backfocus is an average value
// So we can use this to calculate "a" in the above equation
// Note that there are 2 solutions: one giving positive z, the other negative
bool AberrationInspector::processPetzval(TileSelection tileSelection)
{
    float a = 1.0;
    double sum = 0.0;
    double backfocus = m_simMode ? m_simBackfocus : m_backfocus;
    double sign = (backfocus < 0.0) ? -1.0 : 1.0;
    backfocus = std::abs(backfocus);
    switch (tileSelection)
    {
        case TileSelection::TILES_ALL:
            // Use all tiles
            for (int i = 0; i < NUM_TILES; i++)
            {
                if (i == TILE_CM)
                    continue;
                sum += getXYTileCentre(static_cast<tileID>(i)).lengthSquared();
            }
            a = sqrt(sum / (8 * backfocus));
            break;

        case TileSelection::TILES_OUTER_CORNERS:
            // Use tiles 0, 2, 6, 8
            sum += getXYTileCentre(TILE_TL).lengthSquared() +
                   getXYTileCentre(TILE_TR).lengthSquared() +
                   getXYTileCentre(TILE_BL).lengthSquared() +
                   getXYTileCentre(TILE_BR).lengthSquared();
            a = sqrt(sum / (4 * backfocus));
            break;

        case TileSelection::TILES_INNER_DIAMOND:
            // Use tiles 1, 3, 5, 7
            sum += getXYTileCentre(TILE_TM).lengthSquared() +
                   getXYTileCentre(TILE_CL).lengthSquared() +
                   getXYTileCentre(TILE_CR).lengthSquared() +
                   getXYTileCentre(TILE_BM).lengthSquared();
            a = sqrt(sum / (4 * backfocus));
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid tile selection %2").arg(__FUNCTION__).arg(tileSelection);
            return false;
    }

    // Now we have the Petzval equation load a data array of 21 x 21 points
    auto *dataArray = new QSurfaceDataArray;
    float x_step = m_data.sensorWidth * m_data.pixelSize * 2.0 / 21.0;
    float y_step = m_data.sensorHeight * m_data.pixelSize * 2.0 / 21.0;

    m_maxX = std::max(m_maxX, static_cast<float>(m_data.sensorWidth));
    m_maxY = std::max(m_maxY, static_cast<float>(m_data.sensorHeight));

    // Seems like the x values in the data row need to be increasing / descreasing for the dataProxy to work
    for (int j = -10; j < 11; j++)
    {
        auto *newRow = new QSurfaceDataRow;
        float y = y_step * j;
        for (int i = -10; i < 11; i++)
        {
            float x = x_step * i;
            float z = sign * (pow(x / a, 2.0) + pow(y / a, 2.0));
            newRow->append(QSurfaceDataItem({x, y, z}));
            if (i == 10 && j == 10)
            {
                m_maxZ = std::max(m_maxZ, z);
                m_minZ = std::min(m_minZ, z);
            }
        }
        dataArray->append(newRow);
    }
    m_petzvalProxy->resetArray(dataArray);
    return true;
}

// Returns the X, Y centre of the tile in microns
QVector2D AberrationInspector::getXYTileCentre(tileID tile)
{
    const double halfSW = m_data.sensorWidth * m_data.pixelSize / 2.0;
    const double halfSH = m_data.sensorHeight * m_data.pixelSize / 2.0;
    const double halfTS = m_data.tileWidth * m_data.pixelSize / 2.0;

    // Focus calculates the average star position in each tile and passes this to Aberration Inspector as
    // an x, y offset from the center of the tile. If stars are homogenously distributed then the offset would
    // be 0, 0. If they aren't, then offset represents how much to add to the tile centre.
    // A user option (abInsOptCentres) specifies whether to use the offsets.
    double xOffset = 0.0;
    double yOffset = 0.0;
    if (abInsOptCentres->isChecked())
    {
        xOffset = m_tileOffsets[tile].x() * m_data.pixelSize;
        yOffset = m_tileOffsets[tile].y() * m_data.pixelSize;
    }

    switch (tile)
    {
        case TILE_TL:
            return QVector2D(-(halfSW - halfTS) + xOffset, halfSH - halfTS + yOffset);
            break;

        case TILE_TM:
            return QVector2D(xOffset, halfSH - halfTS + yOffset);
            break;

        case TILE_TR:
            return QVector2D(halfSW - halfTS + xOffset, halfSH - halfTS + yOffset);
            break;

        case TILE_CL:
            return QVector2D(-(halfSW - halfTS) + xOffset, yOffset);
            break;

        case TILE_CM:
            return QVector2D(xOffset, yOffset);
            break;

        case TILE_CR:
            return QVector2D(halfSW - halfTS + xOffset, yOffset);
            break;

        case TILE_BL:
            return QVector2D(-(halfSW - halfTS) + xOffset, -(halfSH - halfTS) + yOffset);
            break;

        case TILE_BM:
            return QVector2D(xOffset, -(halfSH - halfTS) + yOffset);
            break;

        case TILE_BR:
            return QVector2D(halfSW - halfTS + xOffset, -(halfSH - halfTS) + yOffset);
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid tile %2").arg(__FUNCTION__).arg(tile);
            return QVector2D(0.0, 0.0);
            break;
    }
}

// Position the labels just outside the sensor so they are always visible
QVector3D AberrationInspector::getLabelCentre(tileID tile)
{
    const double halfSW = m_data.sensorWidth * m_data.pixelSize / 2.0;
    const double halfSH = m_data.sensorHeight * m_data.pixelSize / 2.0;
    const double halfTS = m_data.tileWidth * m_data.pixelSize / 2.0;

    QVector3D point;
    point.setZ(0.0);

    switch (tile)
    {
        case TILE_TL:
            point.setX(-halfSW - halfTS);
            point.setY(halfSH + halfTS);
            break;

        case TILE_TM:
            point.setX(0.0);
            point.setY(halfSH + halfTS);
            break;

        case TILE_TR:
            point.setX(halfSW + halfTS);
            point.setY(halfSH + halfTS);
            break;

        case TILE_CL:
            point.setX(-halfSW - halfTS);
            point.setY(0.0);
            break;

        case TILE_CM:
            point.setX(0.0);
            point.setY(0.0);
            break;

        case TILE_CR:
            point.setX(halfSW + halfTS);
            point.setY(0.0);
            break;

        case TILE_BL:
            point.setX(-halfSW - halfTS);
            point.setY(-halfSH - halfTS);
            break;

        case TILE_BM:
            point.setX(0.0);
            point.setY(-halfSH - halfTS);
            break;

        case TILE_BR:
            point.setX(halfSW + halfTS);
            point.setY(-halfSH - halfTS);
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid tile %2").arg(__FUNCTION__).arg(tile);
            point.setX(0.0);
            point.setY(0.0);
            break;
    }
    // We have the coordinates of the point, so now rotate it...
    return rotatePoint(point);
}

// Returns the vertex of the sensor
// x, y are the sensor corner
// z is calculated from the curve fit of the sensor to a plane
QVector3D AberrationInspector::getSensorVertex(tileID tile)
{
    const double halfSW = m_data.sensorWidth * m_data.pixelSize / 2.0;
    const double halfSH = m_data.sensorHeight * m_data.pixelSize / 2.0;

    QVector3D point;
    point.setZ(0.0);

    switch (tile)
    {
        case TILE_TL:
            point.setX(-halfSW);
            point.setY(halfSH);
            break;

        case TILE_TR:
            point.setX(halfSW);
            point.setY(halfSH);
            break;

        case TILE_BL:
            point.setX(-halfSW);
            point.setY(-halfSH);
            break;

        case TILE_BR:
            point.setX(halfSW);
            point.setY(-halfSH);
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid tile %2").arg(__FUNCTION__).arg(tile);
            point.setX(0.0);
            point.setY(0.0);
            break;
    }
    // We have the coordinates of the point, so now rotate it...
    return rotatePoint(point);
}

// Calculate the backfocus subtracted delta of the passed in tile versus the central tile
// This is really just a translation of the datapoints by the backfocus delta
double AberrationInspector::getBSDelta(tileID tile)
{
    if (tile == TILE_CM)
        return 0.0;
    else
        return m_deltas[tile] - m_backfocus;
}

// Rotate the passed in 3D point based on:
// Sim mode: use the sim slider values for Left-to-Right and Top-to-Bottom tilt
// !Sim mode: use the Left-to-Right and Top-to-Bottom tilt calculated from the focus position deltas
//
// Qt provides the QQuaternion class, although the documentation is very basic.
// More info: https://en.wikipedia.org/wiki/Quaternion
// The QQuaternion class provides a way to do 3D rotations. This is simpler than doing all the 3D
// multiplication manually, although the results would be the same.
// Be careful that multiplication is NOT commutative!
//
// Left-to-Right tilt is a rotation about the y-axis
// Top-to-bottom tilt is a rotation about the x-axis
//
QVector3D AberrationInspector::rotatePoint(QVector3D point)
{
    float LtoRAngle, TtoBAngle;

    if (m_simMode)
    {
        LtoRAngle = std::asin(m_simLRTilt / 100.0);
        TtoBAngle = std::asin(m_simTBTilt / 100.0);
    }
    else
    {
        LtoRAngle = std::asin(m_LRTilt / 100.0);
        TtoBAngle = std::asin(m_TBTilt / 100.0);
    }
    QQuaternion xRotation = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, TtoBAngle * RADIANS2DEGREES);
    QQuaternion yRotation = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, LtoRAngle * RADIANS2DEGREES);
    QQuaternion totalRotation = yRotation * xRotation;
    return totalRotation.rotatedVector(point);
}

// Backfocus simulation slider has changed
void AberrationInspector::simBackfocusChanged(int value)
{
    m_simBackfocus = abs(m_backfocus) * static_cast<double>(value) / 5.0;
    updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

// Left-to-Right tilt simulation slider has changed
void AberrationInspector::simLRTiltChanged(int value)
{
    m_simLRTilt = abs(m_LRTilt) * static_cast<double>(value) / 5.0;
    updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

// Top-to-Bottom tilt simulation slider has changed
void AberrationInspector::simTBTiltChanged(int value)
{
    m_simTBTilt = abs(m_TBTilt) * static_cast<double>(value) / 5.0;
    updateGraphic(static_cast<TileSelection>(abInsTileSelection->currentIndex()));
}

}
