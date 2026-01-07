/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsstackmonitor.h"
#include "fits_debug.h"
#include "ui_livestackmonitor.h"

#include <QStringList>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QStyledItemDelegate>
#include <QStandardItem>
#include <QTimer>

constexpr int CELL_HIGHLIGHT_ROLE = Qt::UserRole + 1;
constexpr int ROW_HIGHLIGHT_ROLE = Qt::UserRole + 2;
const int HIGHLIGHT_DURATION = 3000;

// Subclass QStyledItemDelegate for highlighting
class StackMonitorDelegate : public QStyledItemDelegate
{
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
        {
            QVariant isHighlighted = index.data(CELL_HIGHLIGHT_ROLE);
            QStyleOptionViewItem opt(option);

            if (isHighlighted.toBool())
            {
                opt.backgroundBrush = QBrush(Qt::yellow);
                opt.palette.setColor(QPalette::Text, Qt::black);  // reversed fg
            }

            QStyledItemDelegate::paint(painter, opt, index);
        }
};

SubStatsModel::SubStatsModel(QObject *parent) : QAbstractTableModel(parent)
{
}

int SubStatsModel::rowCount(const QModelIndex &) const
{
    return m_subStatsList.size();
}

QVariant SubStatsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_subStatsList.size())
        return {};

    const SubStats &sub = m_subStatsList.at(index.row());

    bool isHighlighted = m_HighlightedIDs.contains(sub.id) || m_HighlightedCells.contains(index);
    bool isAlignMaster = (index.row() == m_AlignmentMasterRow);

    // Define colors
    QColor alignMasterBg(255, 255, 204);  // pale yellow
    QColor failedBg = QColor(Qt::red).lighter(180); // pale red
    QColor defaultBg = Qt::white;
    QColor defaultFg = Qt::black;

    // Base bg and fg, start with defaults
    QColor bgColor = defaultBg;
    QColor fgColor = defaultFg;

    if (sub.status != SubStatus::InProgress && sub.status != SubStatus::Processed)
        bgColor = failedBg;
    else if (isAlignMaster)
        bgColor = alignMasterBg;

    // Swap foreground and background if highlighted
    if (isHighlighted)
        std::swap(bgColor, fgColor);

    if (role == Qt::BackgroundRole)
        return QBrush(bgColor);

    if (role == Qt::ForegroundRole)
        return QBrush(fgColor);

    if (role == Qt::TextAlignmentRole)
    {
        int col = index.column();
        if (col >= 0 && col < ColumnInfos.size())
        {
            const ColumnInfo &info = ColumnInfos.at(col);
            return static_cast<int>(info.alignment);
        }
    }

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case COL_ID:
                return sub.id;
            case COL_PATHNAME:
                return sub.pathname;
            case COL_FILENAME:
                return sub.filename;
            case COL_CHANNELS:
                return StackMonUtils::displayChannels(sub.channels, sub.morc);
            case COL_STATUS:
                return StackMonUtils::displayOverallStatus(sub.status);
            case COL_WAIT_LOAD_INTERVAL:
                return (sub.waitLoadInterval < 0) ? QVariant() :
                       QString::number(sub.waitLoadInterval, 'f', 2);
            case COL_LOADED:
                return StackMonUtils::displayStatus(sub.loaded);
            case COL_LOADED_INTERVAL:
                return (sub.loaded == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.loadedInterval, 'f', 2);
            case COL_SNR:
                return (sub.loaded == LSStatus::LSStatusUninit) ? QVariant() : QString::number(sub.snr, 'f', 2);
            case COL_PLATE_SOLVED:
                return StackMonUtils::displayStatus(sub.plateSolved);
            case COL_PLATE_SOLVED_INTERVAL:
                return (sub.plateSolved == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.plateSolvedInterval, 'f', 2);
            case COL_HFR:
                return (sub.plateSolved == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.hfr, 'f', 2);;
            case COL_NUM_STARS:
                return (sub.plateSolved == LSStatus::LSStatusUninit) ? QVariant() : sub.numStars;
            case COL_WAIT_STACK_INTERVAL:
                return (sub.waitStackInterval < 0) ? QVariant() :
                       QString::number(sub.waitStackInterval, 'f', 2);
            case COL_CALIBRATED:
                return displayCalibrationStatus(sub.calibrated, sub.dark, sub.flat);
            case COL_CALIBRATED_INTERVAL:
                return (sub.calibrated == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.calibratedInterval, 'f', 2);
            case COL_ALIGNED:
                return StackMonUtils::displayStatus(sub.aligned);
            case COL_ALIGNED_INTERVAL:
                return (sub.aligned == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.alignedInterval, 'f', 2);
            case COL_DX:
                return (sub.aligned == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.dx, 'f', 2);
            case COL_DY:
                return (sub.aligned == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.dy, 'f', 2);
            case COL_ROTATION:
                return (sub.aligned == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.rotation, 'f', 2);
            case COL_STACKED:
                return StackMonUtils::displayStatus(sub.stacked);
            case COL_STACKED_INTERVAL:
                return (sub.stacked == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.stackedInterval, 'f', 2);
            case COL_WEIGHT:
                return (sub.stacked == LSStatus::LSStatusUninit) ? QVariant() :
                       QString::number(sub.weight, 'f', 2);
        }
    }
    return {};
}

QString SubStatsModel::displayCalibrationStatus(const LSStatus &status, const int dark, const int flat) const
{
    LSStatus darkStatus, flatStatus;
    if (dark == 0)
        darkStatus = LSStatus::LSStatusOK;
    else if (dark < 0)
        darkStatus = LSStatus::LSStatusNA;
    else
        darkStatus = LSStatus::LSStatusError;

    if (flat == 0)
        flatStatus = LSStatus::LSStatusOK;
    else if (flat < 0)
        flatStatus = LSStatus::LSStatusNA;
    else
        flatStatus = LSStatus::LSStatusError;

    switch (status)
    {
        case LSStatus::LSStatusOK:
        case LSStatus::LSStatusError:
        case LSStatus::LSStatusNA:
            return QString("D %1   F %2").arg(StackMonUtils::displayStatus(darkStatus))
                   .arg(StackMonUtils::displayStatus(flatStatus));
        case LSStatus::LSStatusUninit:
        default:
            return QString();
    }
}

bool SubStatsModel::setData(const QModelIndex &index, const QVariant &highlight, int role)
{
    if (!index.isValid())
        return false;

    if (role == CELL_HIGHLIGHT_ROLE)
    {
        QPersistentModelIndex persistent(index);
        if (highlight.toBool())
            m_HighlightedCells.insert(persistent);
        else
            m_HighlightedCells.remove(persistent);

        emit dataChanged(index, index, {Qt::BackgroundRole, Qt::ForegroundRole});
        return true;
    }

    if (role == ROW_HIGHLIGHT_ROLE)
    {
        const SubStats &sub = m_subStatsList.at(index.row());

        if (highlight.toBool())
            m_HighlightedIDs.insert(sub.id);
        else
            m_HighlightedIDs.remove(sub.id);

        emit dataChanged(this->index(index.row(), 0), this->index(index.row(), columnCount() - 1),
        {Qt::BackgroundRole, Qt::ForegroundRole});
        return true;
    }
    return false;
}

QVariant SubStatsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section < 0 || section >= COL_COUNT)
        return QAbstractTableModel::headerData(section, orientation, role);

    const ColumnInfo &info = ColumnInfos[section];

    switch (role)
    {
        case Qt::DisplayRole:
            return info.header;
        case Qt::ToolTipRole:
            return info.tooltip;
        case Qt::TextAlignmentRole:
            return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        case Qt::UserRole:
            return info.group;
        default:
            return QAbstractTableModel::headerData(section, orientation, role);
    }
}

int SubStatsModel::addSubStats(const SubStats &stats)
{
    int row = m_subStatsList.size();
    beginInsertRows(QModelIndex(), m_subStatsList.size(), m_subStatsList.size());
    m_subStatsList.append(stats);
    endInsertRows();
    return row;
}

void SubStatsModel::updateSubStats(int id, const SubStats &stats)
{
    for (int i = 0; i < m_subStatsList.size(); ++i)
    {
        if (m_subStatsList[i].id == id)
        {
            m_subStatsList[i] = stats;
            QModelIndex topLeft = index(i, 0);
            QModelIndex bottomRight = index(i, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight);
            break;
        }
    }
}

void SubStatsModel::setAlignmentMasterRow(int row)
{
    m_AlignmentMasterRow = row;
    // Emit dataChanged so view refreshes the row
    emit dataChanged(index(row, 0), index(row, columnCount() - 1), {Qt::BackgroundRole});
}

void SubStatsModel::clear()
{
    beginResetModel();
    m_subStatsList.clear();
    m_AlignmentMasterRow = -1;
    m_HighlightedIDs.clear();
    m_HighlightedCells.clear();
    endResetModel();
}

int SubStatsModel::columnCount(const QModelIndex &) const
{
    return COL_COUNT;
}

// StackMonitor
StackMonitor::StackMonitor(QWidget *parent) : QWidget(parent), ui(new Ui::StackMonitorDialog),
    m_Model(new SubStatsModel(this))
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Window);
    this->setWindowTitle("Live Stacking Monitor (BETA)");

    m_ProxyModel = new QSortFilterProxyModel(this);
    m_ProxyModel->setSourceModel(m_Model);
    m_ProxyModel->setSortRole(Qt::DisplayRole);

    ui->StackMonitorTableView->setModel(m_ProxyModel);
    ui->StackMonitorTableView->sortByColumn(0, Qt::DescendingOrder);

    // Enable column drag/reordering
    ui->StackMonitorTableView->horizontalHeader()->setSectionsMovable(true);

    // Setup header
    auto *header = ui->StackMonitorTableView->horizontalHeader();
    header->setSectionsClickable(true);
    header->setSectionsMovable(true);
    header->setDefaultAlignment(Qt::AlignCenter);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(50);
    header->setStyleSheet("QHeaderView::section { qproperty-alignment: AlignCenter; }");

    // Enable custom context menu on header
    header->setContextMenuPolicy(Qt::CustomContextMenu);

    // Make visible text change with column size
    connect(ui->StackMonitorTableView->horizontalHeader(), &QHeaderView::sectionResized, this, [this]()
    {
        ui->StackMonitorTableView->viewport()->update();
    });

    // Add a menu to allow columns to be shown / hidden
    connect(header, &QHeaderView::customContextMenuRequested, this, [this, header](const QPoint & pos)
    {
        QMenu menu;
        auto model = ui->StackMonitorTableView->model();

        QMap<QString, QMenu*> groupMenus;

        for (int i = 0; i < model->columnCount(); i++)
        {
            QString groupName = model->headerData(i, Qt::Horizontal, Qt::UserRole).toString();
            QString headerName = model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();

            if (groupName.isEmpty())
                groupName = "Miscellaneous";

            if (!groupMenus.contains(groupName))
                groupMenus[groupName] = menu.addMenu(groupName);

            QMenu *groupMenu = groupMenus[groupName];

            QAction *action = groupMenu->addAction(headerName);
            action->setCheckable(true);
            action->setChecked(!ui->StackMonitorTableView->isColumnHidden(i));

            connect(action, &QAction::toggled, this, [this, i](bool checked)
            {
                ui->StackMonitorTableView->setColumnHidden(i, !checked);
            });
        }
        menu.exec(header->mapToGlobal(pos));
    });

    // Connect double-click on header to hide the column
    connect(header, &QHeaderView::sectionDoubleClicked, this, [this](int logicalIndex)
    {
        auto *table = ui->StackMonitorTableView;
        if (logicalIndex >= 0 && logicalIndex < table->model()->columnCount())
            if (!table->isColumnHidden(logicalIndex))
                table->setColumnHidden(logicalIndex, true);
    });

    ui->StackMonitorTableView->setItemDelegate(new StackMonitorDelegate(this));

    // Restore UI settings
    restoreSettings();

    // Highlight checkbox
    connect(ui->StackMonitorHighlight, &QCheckBox::toggled, this, [ = ](bool checked)
    {
        m_Highlight = checked;
        if (!checked)
        {
            // Clear all in-flight highlights
            for (int row = 0; row < m_Model->rowCount(); ++row)
            {
                QModelIndex index = m_Model->index(row, 0);
                if (index.isValid())
                    m_Model->setData(index, false, ROW_HIGHLIGHT_ROLE);

                for (int col = 0; col < m_Model->columnCount(); col++)
                {
                    QModelIndex index = m_Model->index(row, col);
                    if (index.isValid())
                        m_Model->setData(index, false, CELL_HIGHLIGHT_ROLE);
                }
            }
        }
    });

    // Connect close button
    connect(ui->StackMonitorButtonBox, &QDialogButtonBox::clicked, this, [this]()
    {
        this->hide();
    });
}

StackMonitor::~StackMonitor()
{
    delete ui;
}

// Reset the monitor
void StackMonitor::clear()
{
    if (m_Model)
        m_Model->clear();

    // Scroll to top and clear selection
    ui->StackMonitorTableView->scrollToTop();
    ui->StackMonitorTableView->clearSelection();

    // Reset progress bar
    updateProgress();
}

void StackMonitor::initialize(QDateTime timestamp, const QVector<LiveStackFile> &subs)
{
    for (int sub = 0; sub < subs.size(); sub++)
    {
        SubStats stats;
        stats.id = subs[sub].ID;
        stats.pathname = QFileInfo(subs[sub].file).absoluteFilePath();
        stats.filename = QFileInfo(subs[sub].file).fileName();
        stats.channels.push_back(subs[sub].baseChannel);
        stats.channels += subs[sub].channels;
        stats.startTime = timestamp;
        stats.status = SubStatus::InProgress;
        int row = addSub(stats);
        highlightRow(row);
    }
    // Start progress timer
    m_ProgressTimer.start();
    updateProgress();
}

// Add new incoming subs
void StackMonitor::addSubs(QDateTime timestamp, const QVector<LiveStackFile> &subs)
{
    for (int sub = 0; sub < subs.size(); sub++)
    {
        SubStats stats;
        stats.id = subs[sub].ID;
        stats.pathname = QFileInfo(subs[sub].file).absoluteFilePath();
        stats.filename = QFileInfo(subs[sub].file).fileName();
        stats.channels.push_back(subs[sub].baseChannel);
        stats.channels += subs[sub].channels;
        stats.status = SubStatus::InProgress;
        stats.startTime = timestamp;
        int row = addSub(stats);
        highlightRow(row);
    }
    updateProgress();
}

// Update subs with new information as they are processed
void StackMonitor::updateSubs(const QVector<LiveStackFile> &subs, const QVector<LiveStackStageInfo> &stageInfos)
{
    if (subs.size() != stageInfos.size())
    {
        qCDebug(KSTARS_FITS) << "updateSubs: subs and stageInfos size mismatch";
        return;
    }

    for (int i = 0; i < subs.size(); ++i)
    {
        const LiveStackFile &sub = subs[i];
        const LiveStackStageInfo &stageInfo = stageInfos[i];

        int row = findRowForSubID(subs[i].ID);
        if (row < 0)
        {
            qCDebug(KSTARS_FITS) << "Could not find row for sub:" << sub.file << "with ID:" << sub.ID;
            continue;
        }

        const QList<int> columnsToHighlight = applyUpdate(row, stageInfo);
        highlightCells(row, columnsToHighlight);
    }
    updateProgress();
}

void StackMonitor::highlightRow(int sourceRow)
{
    if (!m_Highlight || !m_Model)
        return;

    QModelIndex sourceIndex = m_Model->index(sourceRow, 0);
    if (!sourceIndex.isValid())
        return;

    QPersistentModelIndex pSourceIndex(sourceIndex);
    m_Model->setData(sourceIndex, true, ROW_HIGHLIGHT_ROLE);

    QTimer::singleShot(HIGHLIGHT_DURATION, this, [this, pSourceIndex]()
    {
        if (m_Model && pSourceIndex.isValid())
            m_Model->setData(pSourceIndex, false, ROW_HIGHLIGHT_ROLE);
    });
}

void StackMonitor::highlightCells(int sourceRow, const QList<int> &columns)
{
    if (!m_Highlight || !m_Model)
        return;

    QVector<QPersistentModelIndex> cellsToClear;

    for (int col : columns)
    {
        QModelIndex sourceIndex = m_Model->index(sourceRow, col);
        if (!sourceIndex.isValid())
            continue;

        QPersistentModelIndex persistent(sourceIndex);
        cellsToClear.append(persistent);

        m_Model->setData(sourceIndex, true, CELL_HIGHLIGHT_ROLE);
    }

    // Schedule timer to clear highlights
    QTimer::singleShot(HIGHLIGHT_DURATION, this, [ = ]()
    {
        if (!m_Model)
            return;

        for (const QPersistentModelIndex &persistent : cellsToClear)
            if (persistent.isValid())
                m_Model->setData(persistent, false, CELL_HIGHLIGHT_ROLE);
    });
}

int StackMonitor::addSub(const SubStats &stats)
{
    return m_Model->addSubStats(stats);
}

int StackMonitor::getIDForSub(const QString &sub) const
{
    for (const SubStats &s : m_Model->getSubStats())
    {
        if (s.pathname == sub || s.filename == sub)
            return s.id;
    }
    return -1;
}

int StackMonitor::findRowForSubID(int subID) const
{
    for (int i = 0; i < m_Model->rowCount(); ++i)
    {
        QModelIndex index = m_Model->index(i, COL_ID);
        if (index.isValid() && index.data().toInt() == subID)
            return i;
    }
    return -1;
}

QList<int> StackMonitor::applyUpdate(int row, const LiveStackStageInfo &info)
{
    if (row < 0 || row >= m_Model->rowCount())
        return {};

    SubStats sub = m_Model->getSubStats(row);

    QList<int> columnsToUpdate;

    switch (info.stage)
    {
        case LSStage::WaitLoad:
            sub.waitLoad = info.status;
            sub.waitLoadTime = info.timestamp;
            sub.waitLoadInterval = sub.startTime.msecsTo(sub.waitLoadTime) / 1000.0;
            columnsToUpdate << COL_WAIT_LOAD_INTERVAL;
            if (info.status == LSStatus::LSStatusError)
            {
                sub.status = SubStatus::FailedWaiting;
                columnsToUpdate << COL_STATUS;
            }
            break;

        case LSStage::Loaded:
            sub.loaded = info.status;
            sub.loadedTime = info.timestamp;
            sub.loadedInterval = sub.waitLoadTime.msecsTo(sub.loadedTime) / 1000.0;
            sub.snr = info.extraData.value("snr", 0.0).toDouble();
            sub.morc = info.extraData.value("morc", -1).toInt();
            columnsToUpdate << COL_CHANNELS << COL_LOADED << COL_LOADED_INTERVAL << COL_SNR;
            if (info.status == LSStatus::LSStatusError)
            {
                sub.status = SubStatus::FailedLoading;
                columnsToUpdate << COL_STATUS;
            }
            break;

        case LSStage::PlateSolved:
            sub.plateSolved = info.status;
            sub.plateSolvedTime = info.timestamp;
            sub.plateSolvedInterval = sub.loadedTime.msecsTo(sub.plateSolvedTime) / 1000.0;
            sub.hfr = info.extraData.value("hfr", -1.0).toDouble();
            sub.numStars = info.extraData.value("numStars", -1).toInt();
            if (info.extraData.value("alignMaster", false).toBool())
                m_Model->setAlignmentMasterRow(row);
            columnsToUpdate << COL_PLATE_SOLVED << COL_PLATE_SOLVED_INTERVAL << COL_HFR << COL_NUM_STARS;
            if (info.status == LSStatus::LSStatusError)
            {
                sub.status = SubStatus::FailedPlateSolving;
                columnsToUpdate << COL_STATUS;
            }
            break;

        case LSStage::WaitStack:
            sub.waitStack = info.status;
            sub.waitStackTime = info.timestamp;
            // Plate solving is optional so if not done, take the time from previous element
            if (sub.plateSolved == LSStatus::LSStatusOK)
                sub.waitStackInterval = sub.plateSolvedTime.msecsTo(sub.waitStackTime) / 1000.0;
            else
                sub.waitStackInterval = sub.loadedTime.msecsTo(sub.waitStackTime) / 1000.0;
            columnsToUpdate << COL_WAIT_STACK_INTERVAL;
            if (info.status == LSStatus::LSStatusError)
            {
                sub.status = SubStatus::FailedWaiting;
                columnsToUpdate << COL_STATUS;
            }
            break;

        case LSStage::Calibrated:
            sub.calibrated = info.status;
            sub.calibratedTime = info.timestamp;
            sub.calibratedInterval = sub.waitStackTime.msecsTo(sub.calibratedTime) / 1000.0;
            sub.dark = info.extraData.value("dark", 0).toInt();
            sub.flat = info.extraData.value("flat", 0).toInt();
            columnsToUpdate << COL_CALIBRATED << COL_CALIBRATED_INTERVAL;
            if (info.status == LSStatus::LSStatusError)
            {
                sub.status = SubStatus::FailedCalibration;
                columnsToUpdate << COL_STATUS;
            }
            break;

        case LSStage::Aligned:
            sub.aligned = info.status;
            sub.alignedTime = info.timestamp;
            sub.alignedInterval = sub.calibratedTime.msecsTo(sub.alignedTime) / 1000.0;
            sub.dx = info.extraData.value("dx", 0.0).toDouble();
            sub.dy = info.extraData.value("dy", 0.0).toDouble();
            sub.rotation = info.extraData.value("rotation", 0.0).toDouble();
            columnsToUpdate << COL_ALIGNED << COL_ALIGNED_INTERVAL << COL_DX << COL_DY << COL_ROTATION;
            if (info.status == LSStatus::LSStatusError)
            {
                sub.status = SubStatus::FailedAlignment;
                columnsToUpdate << COL_STATUS;
            }
            break;

        case LSStage::Stacked:
            sub.stacked = info.status;
            sub.stackedTime = info.timestamp;
            // Alignment is optional so if not done, take the time from previous element
            if (sub.aligned == LSStatus::LSStatusOK)
                sub.stackedInterval = sub.alignedTime.msecsTo(sub.stackedTime) / 1000.0;
            else
                sub.stackedInterval = sub.calibratedTime.msecsTo(sub.stackedTime) / 1000.0;
            sub.weight = info.extraData.value("weight", -1.0).toDouble();
            sub.status = (info.status == LSStatus::LSStatusOK) ? SubStatus::Processed : SubStatus::FailedStacking;
            columnsToUpdate << COL_STACKED << COL_STACKED_INTERVAL << COL_WEIGHT << COL_STATUS;
            break;

        default:
            break;
    }

    // Update model which triggers a table update
    m_Model->updateSubStats(sub.id, sub);
    return columnsToUpdate;
}

void StackMonitor::saveSettings()
{
    // Table view settings
    QStringList visibleColumns;
    QList<int> widths;

    // Table column visibility and widths
    for (int i = 0; i < COL_COUNT; ++i)
    {
        const QString name = ColumnInfos.at(i).header;

        if (!ui->StackMonitorTableView->isColumnHidden(i))
            visibleColumns << name;

        widths << ui->StackMonitorTableView->columnWidth(i);
    }
    Options::setFitsLSMVisibleColumns(visibleColumns);
    Options::setFitsLSMColumnWidths(widths);

    // Table sort column and order
    int column = ui->StackMonitorTableView->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder order = ui->StackMonitorTableView->horizontalHeader()->sortIndicatorOrder();
    Options::setFitsLSMSortColumn(column);
    Options::setFitsLSMSortOrder(order);

    // Highlighting checkbox
    Options::setFitsLSMHighlighting(ui->StackMonitorHighlight->isChecked());

    // Window geometry
    Options::setFitsLSMWindowGeometry(QString(this->saveGeometry().toBase64()));
}

void StackMonitor::restoreSettings()
{
    // Table view settings
    const QStringList visibleColumns = Options::fitsLSMVisibleColumns();
    const QList<int> savedWidths = Options::fitsLSMColumnWidths();

    for (int i = 0; i < COL_COUNT; i++)
    {
        const QString name = ColumnInfos.at(i).header;

        // Restore visibility
        ui->StackMonitorTableView->setColumnHidden(i, !visibleColumns.contains(name));

        // Restore width
        if (i < savedWidths.size())
            ui->StackMonitorTableView->setColumnWidth(i, savedWidths[i]);
    }

    // Restore table sort preferences
    int sortColumn = Options::fitsLSMSortColumn();
    if (sortColumn < 0 || sortColumn >= COL_COUNT)
        sortColumn = 0;
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(Options::fitsLSMSortOrder());
    ui->StackMonitorTableView->sortByColumn(sortColumn, sortOrder);

    // Highlighting checkbox
    ui->StackMonitorHighlight->setChecked(Options::fitsLSMHighlighting());
    m_Highlight = Options::fitsLSMHighlighting();

    // Window geometry
    const QString geometry = Options::fitsLSMWindowGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(QByteArray::fromBase64(geometry.toUtf8()));
    else
    {
        // First run default
        resize(900, 500);
        move(100, 100);
    }
}

// Update the progress bar
void StackMonitor::updateProgress()
{
    int rows = ui->StackMonitorTableView->model()->rowCount();
    if (rows == 0)
    {
        // Table is empty so nothing more to do.
        ui->StackMonitorProgressBar->reset();
        ui->StackMonitorProgressBar->resetFormat();
        return;
    }

    int inProgress = 0;
    for (int row = 0; row < rows; row++)
    {
        QModelIndex idx = ui->StackMonitorTableView->model()->index(row, SubStatsColumn::COL_STATUS);
        QVariant value = ui->StackMonitorTableView->model()->data(idx);

        if (value == StackMonUtils::displayOverallStatus(SubStatus::InProgress))
            inProgress++;
    }

    int processed = rows - inProgress;
    if (inProgress == 0 || processed <= 4)
        ui->StackMonitorProgressBar->setFormat(QString("%p%"));
    else
    {
        // We have enough data to project how long to go before all jobs are complete
        double elapsedSeconds = m_ProgressTimer.elapsed() / 1000.0;
        double rate = processed / elapsedSeconds;
        double remainingSeconds = inProgress / rate;

        QString eta;
        if (remainingSeconds < 60)
            eta = QString("%1s remaining").arg(int(remainingSeconds));
        else if (remainingSeconds < 3600)
        {
            int mins = int(remainingSeconds) / 60;
            int secs = int(remainingSeconds) % 60;
            eta = QString("%1:%2 remaining").arg(mins).arg(secs, 2, 10, QLatin1Char('0'));
        }
        else
        {
            int hours = int(remainingSeconds) / 3600;
            int mins  = (int(remainingSeconds) % 3600) / 60;
            int secs  = int(remainingSeconds) % 60;
            eta = QString("%1:%2:%3 remaining").arg(hours).arg(mins, 2, 10, QLatin1Char('0'))
                  .arg(secs, 2, 10, QLatin1Char('0'));
        }
        ui->StackMonitorProgressBar->setFormat(QString("%p% (%1)").arg(eta));
    }
    ui->StackMonitorProgressBar->setValue(double(processed * 100) / double(rows));
}
