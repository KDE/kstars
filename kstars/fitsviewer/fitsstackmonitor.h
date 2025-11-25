/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"
#include "Options.h"

#include <QDialog>
#include <QAbstractTableModel>
#include <QVector>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QHeaderView>
#include <QPainter>
#include <QStyleOptionHeader>
#include <QVector>
#include <QElapsedTimer>

// Ensure translation domain is set before any i18nc() calls in this header
namespace
{
    const bool _initKStarsDomain = []()
    {
        KLocalizedString::setApplicationDomain("kstars");
        return true;
    }();
}

namespace Ui { class StackMonitorDialog; }

const QString STATUS_GOOD = QStringLiteral("🟢");
const QString STATUS_BAD = QStringLiteral("🔴");
const QString STATUS_NA = QStringLiteral("⚪");

const QString SINGLE = QStringLiteral("");
const QString MONO   = QStringLiteral("Mono");
const QString COLOR  = QStringLiteral("Color");
const QString RED    = QStringLiteral("🟥");
const QString GREEN  = QStringLiteral("🟩");
const QString BLUE   = QStringLiteral("🟦");
const QString LUM    = QStringLiteral("L");

// Overall processing status of a sub
enum class SubStatus
{
    InProgress,
    Processed,
    FailedLoading,
    FailedPlateSolving,
    FailedWaiting,
    FailedCalibration,
    FailedAlignment,
    FailedStacking
};

// Table columns
enum SubStatsColumn
{
    COL_ID = 0,
    COL_PATHNAME,
    COL_FILENAME,
    COL_CHANNELS,
    COL_STATUS,

    COL_WAIT_LOAD_INTERVAL,
    COL_LOADED,
    COL_LOADED_INTERVAL,
    COL_SNR,

    COL_PLATE_SOLVED,
    COL_PLATE_SOLVED_INTERVAL,
    COL_HFR,
    COL_NUM_STARS,

    COL_WAIT_STACK_INTERVAL,

    COL_CALIBRATED,
    COL_CALIBRATED_INTERVAL,

    COL_ALIGNED,
    COL_ALIGNED_INTERVAL,
    COL_DX,
    COL_DY,
    COL_ROTATION,

    COL_STACKED,
    COL_STACKED_INTERVAL,
    COL_WEIGHT,
    COL_COUNT
};

// Column metadata struct
struct ColumnInfo
{
    QString group;
    QString header;
    QString tooltip;
    Qt::Alignment alignment;
};

namespace StackMonUtils
{
// Helper function to display channels associated with a sub
inline QString displayChannels(const QVector<LiveStackChannel> &channels, const int morc)
{
    QString str;
    for (int i = 0; i < channels.size(); i++)
    {
        switch (channels[i])
        {
            case LiveStackChannel::SINGLE:
                if (morc == -1)
                    str.append(SINGLE);
                else if (morc == 3)
                    str.append(COLOR);
                else
                    str.append(MONO);
                break;
            case LiveStackChannel::RED: str.append(RED); break;
            case LiveStackChannel::GREEN: str.append(GREEN); break;
            case LiveStackChannel::BLUE: str.append(BLUE); break;
            case LiveStackChannel::LUM: str.append(LUM); break;
            default: break;
        }
    }
    return str;
}

// Helper function to display the overall status of a sub
inline QString displayOverallStatus(const SubStatus &status)
{
    switch (status)
    {
        case SubStatus::InProgress:
            return ki18nc("Stack monitor status", "In Progress").toString();
        case SubStatus::Processed:
            return STATUS_GOOD;
        case SubStatus::FailedLoading:
            return ki18nc("Stack monitor status", "%1 Failed to Load").subs(STATUS_BAD).toString();
        case SubStatus::FailedPlateSolving:
            return ki18nc("Stack monitor status", "%1 Failed Plate Solving").subs(STATUS_BAD).toString();
        case SubStatus::FailedWaiting:
            return ki18nc("Stack monitor status", "%1 Failed Waiting").subs(STATUS_BAD).toString();
        case SubStatus::FailedCalibration:
            return ki18nc("Stack monitor status", "%1 Failed Calibration").subs(STATUS_BAD).toString();
        case SubStatus::FailedAlignment:
            return ki18nc("Stack monitor status", "%1 Failed Alignment").subs(STATUS_BAD).toString();
        case SubStatus::FailedStacking:
            return ki18nc("Stack monitor status", "%1 Failed to Stack").subs(STATUS_BAD).toString();
        default:
            return QString();
    }
}

inline QString displayStatus(const LSStatus &status)
{
    switch (status)
    {
        case LSStatus::LSStatusOK: return STATUS_GOOD;
        case LSStatus::LSStatusError: return STATUS_BAD;
        case LSStatus::LSStatusNA: return STATUS_NA;
        default: return QString();
    }
}

inline QString channelsHelp(const QString &name)
{
    QString str = i18n("%1 \n%2 = Mono Single Channel\n%3 = Color Single Channel\n%4 = Red Channel\n%5 = Green Channel\n"
                       "%6 = Blue Channel\n%7 = Lum Channel", name, MONO, COLOR, RED, GREEN, BLUE, LUM);
    return str;
}

inline QString statusHelp(const QString &name, const bool useNA=true)
{
    QString str = ki18nc("Stack monitor status help", "%1 \n%2 = OK\n%3 = Error")
                      .subs(name)
                      .subs(displayStatus(LSStatus::LSStatusOK))
                      .subs(displayStatus(LSStatus::LSStatusError))
                      .toString();
    if (useNA)
        str = ki18nc("Stack monitor status help", "%1 \n%2 = Not applicable")
                  .subs(str)
                  .subs(displayStatus(LSStatus::LSStatusNA))
                  .toString();
    return str;
}

inline QString overallStatusHelp(const QString &name)
{
    return ki18nc("Stack monitor overall status help",
                  "%1 \n%2\n%3 = Processed Successfully\n%4\n%5\n%6\n%7\n%8\n%9")
        .subs(name)
        .subs(displayOverallStatus(SubStatus::InProgress))
        .subs(displayOverallStatus(SubStatus::Processed))
        .subs(displayOverallStatus(SubStatus::FailedLoading))
        .subs(displayOverallStatus(SubStatus::FailedPlateSolving))
        .subs(displayOverallStatus(SubStatus::FailedWaiting))
        .subs(displayOverallStatus(SubStatus::FailedCalibration))
        .subs(displayOverallStatus(SubStatus::FailedAlignment))
        .subs(displayOverallStatus(SubStatus::FailedStacking))
        .toString();
}

} // namespace StackMonUtils

// Single source of truth for all table columns
static const QVector<ColumnInfo> ColumnInfos =
{
    { "",              i18nc("Column header", "ID"),
                       i18nc("Tooltip for ID column", "Unique subframe identifier"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "",              i18nc("Column header", "Pathname"),
                       i18nc("Tooltip for Pathname column", "Full file path"),
                                                    Qt::AlignLeft | Qt::AlignVCenter },
    { "",              i18nc("Column header", "Filename"),
                       i18nc("Tooltip for Filename column", "Filename only"),
                                                    Qt::AlignLeft | Qt::AlignVCenter },
    { "",              i18nc("Column header", "Channel(s)"),
          StackMonUtils::channelsHelp(i18nc("Tooltip for Channels column", "Channel(s)")),
                                                    Qt::AlignCenter },
    { "",              i18nc("Column header", "Overall\nStatus"),
          StackMonUtils::overallStatusHelp(i18nc("Tooltip for Overall Status column", "Overall processing status")),
                                                    Qt::AlignCenter },

    { "Loading",       i18nc("Column header", "Load Wait\nTime(s)"),
                       i18nc("Tooltip for Load Wait Time column", "Wait time from subs detected until loading starts"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Loading",       i18nc("Column header", "Loading\nStatus"),
          StackMonUtils::statusHelp(i18nc("Tooltip for Loading Status column", "Loading status"), false),
                                                    Qt::AlignCenter },
    { "Loading",       i18nc("Column header", "Loading\nTime(s)"),
                       i18nc("Tooltip for Loading Time column", "Time to load"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Loading",       i18nc("Column header", "SNR"),
                       i18nc("Tooltip for SNR column", "Signal-to-noise ratio"),
                                                    Qt::AlignRight | Qt::AlignVCenter },

    { "Plate Solving", i18nc("Column header", "Plate Solve\nStatus"),
          StackMonUtils::statusHelp(i18nc("Tooltip for Plate Solve Status column", "Plate solving status")),
                                                    Qt::AlignCenter },
    { "Plate Solving", i18nc("Column header", "Plate Solve\nTime(s)"),
                       i18nc("Tooltip for Plate Solve Time column", "Time to plate solve"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Plate Solving", i18nc("Column header", "HFR"),
                       i18nc("Tooltip for HFR column", "Average HFR of stars"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Plate Solving", i18nc("Column header", "Num Stars"),
                       i18nc("Tooltip for Num Stars column", "Number of stars detected"),
                                                    Qt::AlignRight | Qt::AlignVCenter },

    { "Stack Waiting", i18nc("Column header", "Stack Wait\nTime(s)"),
                       i18nc("Tooltip for Stack Wait Time column", "Wait time for sufficient subs to start stacking"),
                                                    Qt::AlignRight | Qt::AlignVCenter },

    { "Calibration",   i18nc("Column header", "Calibration\nStatus"),
          StackMonUtils::statusHelp(i18nc("Tooltip for Calibration Status column", "Calibration status")),
                                                    Qt::AlignCenter },

    { "Calibration",   i18nc("Column header", "Calibration\nTime(s)"),
                       i18nc("Tooltip for Calibration Time column", "Time to calibrate"),
                                                    Qt::AlignRight | Qt::AlignVCenter },

    { "Alignment",     i18nc("Column header", "Alignment\nStatus"),
          StackMonUtils::statusHelp(i18nc("Tooltip for Alignment Status column", "Alignment status")),
                                                    Qt::AlignCenter },
    { "Alignment",     i18nc("Column header", "Alignment\nTime(s)"),
                       i18nc("Tooltip for Alignment Time column", "Time to align"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Alignment",     i18nc("Column header", "Δx"),
                       i18nc("Tooltip for Δx column", "X-axis shift in pixels compared to alignment master"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Alignment",     i18nc("Column header", "Δy"),
                       i18nc("Tooltip for Δy column", "Y-axis shift in pixels compared to alignment master"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Alignment",     i18nc("Column header", "Rot(°)"),
                       i18nc("Tooltip for Rotation column", "Rotation angle in degrees compared to alignment master"),
                                                    Qt::AlignRight | Qt::AlignVCenter },

    { "Stacking",      i18nc("Column header", "Stacking\nStatus"),
          StackMonUtils::statusHelp(i18nc("Tooltip for Stacking Status column", "Stacking status"), false),
                                                    Qt::AlignCenter },
    { "Stacking",      i18nc("Column header", "Stacking\nTime(s)"),
                       i18nc("Tooltip for Stacking Time column", "Time to stack (s)"),
                                                    Qt::AlignRight | Qt::AlignVCenter },
    { "Stacking",      i18nc("Column header", "Stacking\nWeight"),
                       i18nc("Tooltip for Stacking Weight column", "Weight used in stacking"),
                                                    Qt::AlignRight | Qt::AlignVCenter }
};

// Structure holding statistics on a sub
struct SubStats
{
    int id = 0;
    QString pathname;
    QString filename;
    QVector<LiveStackChannel> channels;
    QDateTime startTime;
    SubStatus status = SubStatus::InProgress;

    LSStatus waitLoad = LSStatus::LSStatusUninit;
    QDateTime waitLoadTime;
    double waitLoadInterval = -1.0;

    LSStatus loaded = LSStatus::LSStatusUninit;
    QDateTime loadedTime;
    double loadedInterval = -1.0;
    double snr = -1.0;
    int morc = -1;

    LSStatus plateSolved = LSStatus::LSStatusUninit;
    QDateTime plateSolvedTime;
    double plateSolvedInterval = -1.0;
    double hfr = -1.0;
    int numStars = -1;

    LSStatus waitStack = LSStatus::LSStatusUninit;
    QDateTime waitStackTime;
    double waitStackInterval = -1.0;

    LSStatus calibrated = LSStatus::LSStatusUninit;
    QDateTime calibratedTime;
    double calibratedInterval = -1.0;
    int dark = -1;
    int flat = -1;

    LSStatus aligned = LSStatus::LSStatusUninit;
    QDateTime alignedTime;
    double alignedInterval = -1.0;
    double dx = 0.0;
    double dy = 0.0;
    double rotation = 0.0;

    LSStatus stacked = LSStatus::LSStatusUninit;
    QDateTime stackedTime;
    double stackedInterval = -1.0;
    double weight = -1.0;
};

/**
 * @class SubStatsModel
 * @brief Model providing tabular data for the Live Stacking Monitor.
 *
 * SubStatsModel is a QAbstractTableModel implementation that maintains
 * a list of per-subframe statistics (SubStats) used by the Live Stacker.
 * Each row represents one subframe, and each column represents a specific
 * measurement or processing stage (e.g., SNR, HFR, alignment, stacking, etc.).
 *
 * The model supports:
 *   - Adding and updating SubStats entries as new subframes are processed.
 *   - Tracking and visually highlighting rows or individual cells when
 *     their values change or a new subframe is added.
 *   - Indicating a special “alignment master” row.
 *
 * The model emits dataChanged() as needed to update the table view
 * and uses custom data roles (e.g., HighlightRole) to control cell
 * background and foreground colors.
 *
 * Typical usage:
 *   - The Live Stacking Monitor view displays this model via a proxy.
 *   - StackMonitor calls addSubStats() when a new sub arrives,
 *     then triggers highlightRow() or highlightCells() to animate changes.
 */
class SubStatsModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    explicit SubStatsModel(QObject *parent = nullptr);

    /**
     * @brief Override function to return row count in the table.
     * @param parent
     * @return row count
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Override function to return column count in the table.
     * @param parent
     * @return column count
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Override function to draw data.
     * @param index
     * @param role is the type of display role
     * @return data to draw
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Override function to draw header data.
     * @param section is the section of column belongs to
     * @param orientation
     * @param display role
     * @return data to draw
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /**
     * @brief Override function to set data.
     * @param index
     * @param value
     * @param role
     * @return whether function was successful (or not)
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    /**
     * @brief Add a new sub to SubStats and the table
     * @param stats to add
     * @return row in Substats that was added
     */
    int addSubStats(const SubStats &stats);

    /**
     * @brief Update sub
     * @param Sub ID
     * @param statistics to update
     */
    void updateSubStats(const int id, const SubStats &stats);

    /**
     * @brief Get the Alignment Master row
     * @return row
     */
    int getAlignmentMasterRow() const
    {
        return m_AlignmentMasterRow;
    }

    /**
     * @brief Set the Alignment Master row
     * @param row
     */
    void setAlignmentMasterRow(int row);

    /**
     * @brief Clear the table
     */
    void clear();

    /**
     * @brief Get the Sub Stats for the passed in row
     * @param row
     * @return Sub Stats structure
     */
    const SubStats &getSubStats(int row) const
    {
        return m_subStatsList.at(row);
    }

    /**
     * @brief Get all the Sub Stats
     * @return Sub Stats structures
     */
    const QVector<SubStats> &getSubStats() const
    {
        return m_subStatsList;
    };

    /**
     * @brief Display the overall status of the sub
     * @param status
     * @return Status display string
     */
    QString displayOverallStatus(const SubStatus &status) const;

  private:
    QString displayStatus(const LSStatus &status) const;
    QString displayCalibrationStatus(const LSStatus &status, const int dark, const int flat) const;
    QVector<SubStats> m_subStatsList;
    int m_AlignmentMasterRow { -1 };
    QSet<int> m_HighlightedIDs;
    QSet<QPersistentModelIndex> m_HighlightedCells;
};

/**
 * @class StackMonitor
 * @brief Widget for monitoring and highlighting the status of live stacked subframes.
 *
 * StackMonitor is a QWidget that provides a table view of subframes (subs) processed
 * during live stacking. Each row represents a single subframe and displays various
 * metrics such as load status, alignment, HFR, SNR, stacking, and weight.
 *
 * Key responsibilities:
 *   - Initialize and add subframes as they arrive (`initialize()`, `addSubs()`).
 *   - Update subframe metrics in real time (`updateSubs()`).
 *   - Highlight entire rows or individual cells to indicate new or updated values.
 *   - Track an "alignment master" row for reference.
 *   - Maintain progress tracking and visual updates using timers.
 *   - Persist and restore user settings for table state.
 *
 * The class uses a SubStatsModel as the underlying data model and a
 * QSortFilterProxyModel for sorting/filtering in the view. Highlights are stored
 * in the model and displayed using custom roles for row and cell-level changes.
 *
 * Typical usage:
 *   - Live stacking code calls `initialize()` when a new batch of subs arrives.
 *   - As subs are processed, `updateSubs()` is called to update metrics and trigger
 *     visual highlights.
 *   - `highlightRow()` and `highlightCells()` provide transient visual feedback
 *     to the user for added or updated subs.
 */
class StackMonitor : public QWidget
{
    Q_OBJECT
  public:
    explicit StackMonitor(QWidget *parent = nullptr);
    ~StackMonitor();

    /**
     * @brief Clear (reset) the Monitor
     */
    void clear();

    /**
     * @brief Save monitor settings
     */
    void saveSettings();

  public slots:
    /**
     * @brief Initialize the Monitor
     * @param timestamp of call
     * @param list of initial subs in the dir being watched
     */
    void initialize(QDateTime timestamp, const QVector<LiveStackFile> &subs);

    /**
     * @brief addSubs
     * @param timestamp of call
     * @param subs is a list of subs to add
     */
    void addSubs(QDateTime timestamp, const QVector<LiveStackFile> &subs);

    /**
     * @brief updateSubs
     * @param list of subs to update
     * @param infos is a list of information about the update
     */
    void updateSubs(const QVector<LiveStackFile> &subs, const QVector<LiveStackStageInfo> &infos);

  private:
    int addSub(const SubStats &stats);
    void highlightRow(int row);
    void highlightCells(int row, const QList<int> &columns);
    int findRowForSubID(int subID) const;
    QList<int> applyUpdate(int row, const LiveStackStageInfo &info);
    int getIDForSub(const QString &sub) const;
    void restoreSettings();
    void updateProgress();

    Ui::StackMonitorDialog *ui;
    SubStatsModel *m_Model;
    QSortFilterProxyModel *m_ProxyModel = nullptr;
    bool m_Highlight { true };
    QElapsedTimer m_ProgressTimer;
};
