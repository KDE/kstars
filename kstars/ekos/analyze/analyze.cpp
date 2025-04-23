/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "analyze.h"

#include <knotification.h>
#include <QDateTime>
#include <QShortcut>
#include <QtGlobal>
#include <QColor>

#include "auxiliary/kspaths.h"
#include "dms.h"
#include "ekos/manager.h"
#include "ekos/focus/curvefit.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsviewer.h"
#include "ksmessagebox.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "qcustomplot.h"

#include <ekos_analyze_debug.h>
#include <version.h>
#include <QDesktopServices>
#include <QFileDialog>

// Subclass QCPAxisTickerDateTime, so that times are offset from the start
// of the log, instead of being offset from the UNIX 0-seconds time.
class OffsetDateTimeTicker : public QCPAxisTickerDateTime
{
    public:
        void setOffset(double offset)
        {
            timeOffset = offset;
        }
        QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
        {
            Q_UNUSED(precision);
            Q_UNUSED(formatChar);
            // Seconds are offset from the unix origin by
            return locale.toString(keyToDateTime(tick + timeOffset).toTimeSpec(mDateTimeSpec), mDateTimeFormat);
        }
    private:
        double timeOffset = 0;
};

namespace
{

// QDateTime is written to file with this format.
QString timeFormat = "yyyy-MM-dd hh:mm:ss.zzz";

// The resolution of the scroll bar.
constexpr int MAX_SCROLL_VALUE = 10000;

// Half the height of a timeline line.
// That is timeline lines are horizontal bars along y=1 or y=2 ... and their
// vertical widths are from y-halfTimelineHeight to y+halfTimelineHeight.
constexpr double halfTimelineHeight = 0.35;

// These are initialized in initStatsPlot when the graphs are added.
// They index the graphs in statsPlot, e.g. statsPlot->graph(HFR_GRAPH)->addData(...)
int HFR_GRAPH = -1;
int TEMPERATURE_GRAPH = -1;
int FOCUS_POSITION_GRAPH = -1;
int NUM_CAPTURE_STARS_GRAPH = -1;
int MEDIAN_GRAPH = -1;
int ECCENTRICITY_GRAPH = -1;
int NUMSTARS_GRAPH = -1;
int SKYBG_GRAPH = -1;
int SNR_GRAPH = -1;
int RA_GRAPH = -1;
int DEC_GRAPH = -1;
int RA_PULSE_GRAPH = -1;
int DEC_PULSE_GRAPH = -1;
int DRIFT_GRAPH = -1;
int RMS_GRAPH = -1;
int CAPTURE_RMS_GRAPH = -1;
int MOUNT_RA_GRAPH = -1;
int MOUNT_DEC_GRAPH = -1;
int MOUNT_HA_GRAPH = -1;
int AZ_GRAPH = -1;
int ALT_GRAPH = -1;
int PIER_SIDE_GRAPH = -1;
int TARGET_DISTANCE_GRAPH = -1;

// This one is in timelinePlot.
int ADAPTIVE_FOCUS_GRAPH = -1;

// Initialized in initGraphicsPlot().
int FOCUS_GRAPHICS = -1;
int FOCUS_GRAPHICS_FINAL = -1;
int FOCUS_GRAPHICS_CURVE = -1;
int GUIDER_GRAPHICS = -1;

// Brushes used in the timeline plot.
const QBrush temporaryBrush(Qt::green, Qt::DiagCrossPattern);
const QBrush timelineSelectionBrush(QColor(255, 100, 100, 150), Qt::SolidPattern);
const QBrush successBrush(Qt::green, Qt::SolidPattern);
const QBrush failureBrush(Qt::red, Qt::SolidPattern);
const QBrush offBrush(Qt::gray, Qt::SolidPattern);
const QBrush progressBrush(Qt::blue, Qt::SolidPattern);
const QBrush progress2Brush(QColor(0, 165, 255), Qt::SolidPattern);
const QBrush progress3Brush(Qt::cyan, Qt::SolidPattern);
const QBrush progress4Brush(Qt::darkGreen, Qt::SolidPattern);
const QBrush stoppedBrush(Qt::yellow, Qt::SolidPattern);
const QBrush stopped2Brush(Qt::darkYellow, Qt::SolidPattern);

// Utility to checks if a file exists and is not a directory.
bool fileExists(const QString &path)
{
    QFileInfo info(path);
    return info.exists() && info.isFile();
}

// Utilities to go between a mount status and a string.
// Move to inditelescope.h/cpp?
const QString mountStatusString(ISD::Mount::Status status)
{
    switch (status)
    {
        case ISD::Mount::MOUNT_IDLE:
            return i18n("Idle");
        case ISD::Mount::MOUNT_PARKED:
            return i18n("Parked");
        case ISD::Mount::MOUNT_PARKING:
            return i18n("Parking");
        case ISD::Mount::MOUNT_SLEWING:
            return i18n("Slewing");
        case ISD::Mount::MOUNT_MOVING:
            return i18n("Moving");
        case ISD::Mount::MOUNT_TRACKING:
            return i18n("Tracking");
        case ISD::Mount::MOUNT_ERROR:
            return i18n("Error");
    }
    return i18n("Error");
}

ISD::Mount::Status toMountStatus(const QString &str)
{
    if (str == i18n("Idle"))
        return ISD::Mount::MOUNT_IDLE;
    else if (str == i18n("Parked"))
        return ISD::Mount::MOUNT_PARKED;
    else if (str == i18n("Parking"))
        return ISD::Mount::MOUNT_PARKING;
    else if (str == i18n("Slewing"))
        return ISD::Mount::MOUNT_SLEWING;
    else if (str == i18n("Moving"))
        return ISD::Mount::MOUNT_MOVING;
    else if (str == i18n("Tracking"))
        return ISD::Mount::MOUNT_TRACKING;
    else
        return ISD::Mount::MOUNT_ERROR;
}

// Returns the stripe color used when drawing the capture timeline for various filters.
// TODO: Not sure how to internationalize this.
bool filterStripeBrush(const QString &filter, QBrush *brush)
{
    const QRegularExpression::PatternOption c = QRegularExpression::CaseInsensitiveOption;

    const QString rPattern("^(red|r)$");
    if (QRegularExpression(rPattern, c).match(filter).hasMatch())
    {
        *brush = QBrush(Qt::red, Qt::SolidPattern);
        return true;
    }
    const QString gPattern("^(green|g)$");
    if (QRegularExpression(gPattern, c).match(filter).hasMatch())
    {
        *brush = QBrush(Qt::green, Qt::SolidPattern);
        return true;
    }
    const QString bPattern("^(blue|b)$");
    if (QRegularExpression(bPattern, c).match(filter).hasMatch())
    {
        *brush = QBrush(Qt::blue, Qt::SolidPattern);
        return true;
    }
    const QString hPattern("^(ha|h|h-a|h_a|h-alpha|hydrogen|hydrogen_alpha|hydrogen-alpha|h_alpha|halpha)$");
    if (QRegularExpression(hPattern, c).match(filter).hasMatch())
    {
        *brush = QBrush(Qt::darkRed, Qt::SolidPattern);
        return true;
    }
    const QString oPattern("^(oiii|oxygen|oxygen_3|oxygen-3|oxygen_iii|oxygen-iii|o_iii|o-iii|o_3|o-3|o3)$");
    if (QRegularExpression(oPattern, c).match(filter).hasMatch())
    {
        *brush = QBrush(Qt::cyan, Qt::SolidPattern);
        return true;
    }
    const QString
    sPattern("^(sii|sulphur|sulphur_2|sulphur-2|sulphur_ii|sulphur-ii|sulfur|sulfur_2|sulfur-2|sulfur_ii|sulfur-ii|s_ii|s-ii|s_2|s-2|s2)$");
    if (QRegularExpression(sPattern, c).match(filter).hasMatch())
    {
        // Pink.
        *brush = QBrush(QColor(255, 182, 193), Qt::SolidPattern);
        return true;
    }
    const QString lPattern("^(lpr|L|UV-IR cut|UV-IR|white|monochrome|broadband|clear|focus|luminance|lum|lps|cls)$");
    if (QRegularExpression(lPattern, c).match(filter).hasMatch())
    {
        *brush = QBrush(Qt::white, Qt::SolidPattern);
        return true;
    }
    return false;
}

// Used when searching for FITS files to display.
// If filename isn't found as is, it tries Options::analyzeAlternativeImageDirectory() in several ways
// e.g. if filename = /1/2/3/4/name is not found, then try $dir/name,
// then $dir/4/name, then $dir/3/4/name,
// then $dir/2/3/4/name, and so on.
// If it cannot find the FITS file, it returns an empty string, otherwise it returns
// the full path where the file was found.
QString findFilename(const QString &filename)
{
    const QString &alternateDirectory = Options::analyzeAlternativeDirectoryName();

    // Try the origial full path.
    QFileInfo info(filename);
    if (info.exists() && info.isFile())
        return filename;

    // Try putting the filename at the end of the full path onto alternateDirectory.
    QString name = info.fileName();
    QString temp = QString("%1/%2").arg(alternateDirectory, name);
    if (fileExists(temp))
        return temp;

    // Try appending the filename plus the ending directories onto alternateDirectory.
    int size = filename.size();
    int searchBackFrom = size - name.size();
    int num = 0;
    while (searchBackFrom >= 0)
    {
        int index = filename.lastIndexOf('/', searchBackFrom);
        if (index < 0)
            break;

        QString temp2 = QString("%1%2").arg(alternateDirectory, filename.right(size - index));
        if (fileExists(temp2))
            return temp2;

        searchBackFrom = index - 1;

        // Paranoia
        if (++num > 20)
            break;
    }
    return "";
}

// This is an exhaustive search for now.
// This is reasonable as the number of sessions should be limited.
template <class T>
class IntervalFinder
{
    public:
        IntervalFinder() {}
        ~IntervalFinder() {}
        void add(T value)
        {
            intervals.append(value);
        }
        void clear()
        {
            intervals.clear();
        }
        QList<T> find(double t)
        {
            QList<T> result;
            for (const auto &interval : intervals)
            {
                if (t >= interval.start && t <= interval.end)
                    result.push_back(interval);
            }
            return result;
        }
        // Finds the interval AFTER t, not including t
        T *findNext(double t)
        {
            double bestStart = 1e7;
            T *result = nullptr;
            for (auto &interval : intervals)
            {
                if (interval.start > t && interval.start < bestStart)
                {
                    bestStart = interval.start;
                    result = &interval;
                }
            }
            return result;
        }
        // Finds the interval BEFORE t, not including t
        T *findPrevious(double t)
        {
            double bestStart = -1e7;
            T *result = nullptr;
            for (auto &interval : intervals)
            {
                if (interval.start < t && interval.start > bestStart)
                {
                    bestStart = interval.start;
                    result = &interval;
                }
            }
            return result;
        }
    private:
        QList<T> intervals;
};

IntervalFinder<Ekos::Analyze::CaptureSession> captureSessions;
IntervalFinder<Ekos::Analyze::FocusSession> focusSessions;
IntervalFinder<Ekos::Analyze::GuideSession> guideSessions;
IntervalFinder<Ekos::Analyze::MountSession> mountSessions;
IntervalFinder<Ekos::Analyze::AlignSession> alignSessions;
IntervalFinder<Ekos::Analyze::MountFlipSession> mountFlipSessions;
IntervalFinder<Ekos::Analyze::SchedulerJobSession> schedulerJobSessions;

}  // namespace

namespace Ekos
{

// RmsFilter computes the RMS error of a 2-D sequence. Input the x error and y error
// into newSample(). It returns the sqrt of a moving average of the squared
// errors averaged over 40 samples.
// It's used to compute RMS guider errors, where x and y would be RA and DEC errors.
class RmsFilter
{
    public:
        RmsFilter(int size = 40) : m_WindowSize(size) {}

        double newSample(double x, double y)
        {
            m_XData.push_back(x);
            m_YData.push_back(y);
            m_XSum += x;
            m_YSum += y;
            m_XSumOfSquares += x * x;
            m_YSumOfSquares += y * y;

            if (m_XData.size() > m_WindowSize)
            {
                double oldValue = m_XData.front();
                m_XData.pop_front();
                m_XSum -= oldValue;
                m_XSumOfSquares -= oldValue * oldValue;

                oldValue = m_YData.front();
                m_YData.pop_front();
                m_YSum -= oldValue;
                m_YSumOfSquares -= oldValue * oldValue;
            }
            const int size = m_XData.size();
            if (size < 2)
                return 0.0;

            const double xVariance = (m_XSumOfSquares - (m_XSum * m_XSum) / size) / (size - 1);
            const double yVariance = (m_YSumOfSquares - (m_YSum * m_YSum) / size) / (size - 1);
            return std::sqrt(xVariance + yVariance);
        }

        void resetFilter()
        {
            m_XSum = 0;
            m_YSum = 0;
            m_XSumOfSquares = 0;
            m_YSumOfSquares = 0;
            m_XData.clear();
            m_YData.clear();
        }

    private:
        std::deque<double> m_XData;
        std::deque<double> m_YData;

        unsigned long m_WindowSize;
        double m_XSum = 0, m_YSum = 0;
        double m_XSumOfSquares = 0, m_YSumOfSquares = 0;
};

bool Analyze::eventFilter(QObject *obj, QEvent *ev)
{
    // Quit if click wasn't on a QLineEdit.
    if (qobject_cast<QLineEdit*>(obj) == nullptr)
        return false;

    // This filter only applies to single or double clicks.
    if (ev->type() != QEvent::MouseButtonDblClick && ev->type() != QEvent::MouseButtonPress)
        return false;

    auto axisEntry = yAxisMap.find(obj);
    if (axisEntry == yAxisMap.end())
        return false;

    const bool isRightClick = (ev->type() == QEvent::MouseButtonPress) &&
                              (static_cast<QMouseEvent*>(ev)->button() == Qt::RightButton);
    const bool isControlClick = (ev->type() == QEvent::MouseButtonPress) &&
                                (static_cast<QMouseEvent*>(ev)->modifiers() &
                                 Qt::KeyboardModifier::ControlModifier);
    const bool isShiftClick = (ev->type() == QEvent::MouseButtonPress) &&
                              (static_cast<QMouseEvent*>(ev)->modifiers() &
                               Qt::KeyboardModifier::ShiftModifier);

    if (ev->type() == QEvent::MouseButtonDblClick || isRightClick || isControlClick || isShiftClick)
    {
        startYAxisTool(axisEntry->first, axisEntry->second);
        clickTimer.stop();
        return true;
    }
    else if (ev->type() == QEvent::MouseButtonPress)
    {
        clickTimer.setSingleShot(true);
        clickTimer.setInterval(250);
        clickTimer.start();
        m_ClickTimerInfo = axisEntry->second;
        // Wait 0.25 seconds to see if this is a double click or just a single click.
        connect(&clickTimer, &QTimer::timeout, this, [&]()
        {
            m_YAxisTool.reject();
            if (m_ClickTimerInfo.checkBox && !m_ClickTimerInfo.checkBox->isChecked())
            {
                // Enable the graph.
                m_ClickTimerInfo.checkBox->setChecked(true);
                statsPlot->graph(m_ClickTimerInfo.graphIndex)->setVisible(true);
                statsPlot->graph(m_ClickTimerInfo.graphIndex)->addToLegend();
            }
            userSetLeftAxis(m_ClickTimerInfo.axis);
        });
        return true;
    }
    return false;
}

Analyze::Analyze() : m_YAxisTool(this)
{
    setupUi(this);

    captureRms.reset(new RmsFilter);
    guiderRms.reset(new RmsFilter);

    initInputSelection();
    initTimelinePlot();

    initStatsPlot();
    connect(&m_YAxisTool, &YAxisTool::axisChanged, this, &Analyze::userChangedYAxis);
    connect(&m_YAxisTool, &YAxisTool::leftAxisChanged, this, &Analyze::userSetLeftAxis);
    connect(&m_YAxisTool, &YAxisTool::axisColorChanged, this, &Analyze::userSetAxisColor);
    qApp->installEventFilter(this);

    initGraphicsPlot();
    fullWidthCB->setChecked(true);
    keepCurrentCB->setChecked(true);
    runtimeDisplay = true;
    fullWidthCB->setVisible(true);
    fullWidthCB->setDisabled(false);

    // Initialize the checkboxes that allow the user to make (in)visible
    // each of the 4 main displays in Analyze.
    detailsCB->setChecked(true);
    statsCB->setChecked(true);
    graphsCB->setChecked(true);
    timelineCB->setChecked(true);
    setVisibility();
    connect(timelineCB, &QCheckBox::stateChanged, this, &Analyze::setVisibility);
    connect(graphsCB, &QCheckBox::stateChanged, this, &Analyze::setVisibility);
    connect(statsCB, &QCheckBox::stateChanged, this, &Analyze::setVisibility);
    connect(detailsCB, &QCheckBox::stateChanged, this, &Analyze::setVisibility);

    connect(fullWidthCB, &QCheckBox::toggled, [ = ](bool checked)
    {
        if (checked)
            this->replot();
    });

    initStatsCheckboxes();

    connect(zoomInB, &QPushButton::clicked, this, &Analyze::zoomIn);
    connect(zoomOutB, &QPushButton::clicked, this, &Analyze::zoomOut);
    connect(prevSessionB, &QPushButton::clicked, this, &Analyze::previousTimelineItem);
    connect(nextSessionB, &QPushButton::clicked, this, &Analyze::nextTimelineItem);
    connect(timelinePlot, &QCustomPlot::mousePress, this, &Analyze::timelineMousePress);
    connect(timelinePlot, &QCustomPlot::mouseDoubleClick, this, &Analyze::timelineMouseDoubleClick);
    connect(timelinePlot, &QCustomPlot::mouseWheel, this, &Analyze::timelineMouseWheel);
    connect(statsPlot, &QCustomPlot::mousePress, this, &Analyze::statsMousePress);
    connect(statsPlot, &QCustomPlot::mouseDoubleClick, this, &Analyze::statsMouseDoubleClick);
    connect(statsPlot, &QCustomPlot::mouseMove, this, &Analyze::statsMouseMove);
    connect(analyzeSB, &QScrollBar::valueChanged, this, &Analyze::scroll);
    analyzeSB->setRange(0, MAX_SCROLL_VALUE);
    connect(keepCurrentCB, &QCheckBox::stateChanged, this, &Analyze::keepCurrent);

    setupKeyboardShortcuts(this);

    reset();
    replot();
}

void Analyze::setVisibility()
{
    detailsWidget->setVisible(detailsCB->isChecked());
    statsGridWidget->setVisible(statsCB->isChecked());
    timelinePlot->setVisible(timelineCB->isChecked());
    statsPlot->setVisible(graphsCB->isChecked());
    replot();
}

// Mouse wheel over the Timeline plot causes an x-axis zoom.
void Analyze::timelineMouseWheel(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0)
        zoomIn();
    else if (event->angleDelta().y() < 0)
        zoomOut();
}

// This callback is used so that when keepCurrent is checked, we replot immediately.
// The actual keepCurrent work is done in replot().
void Analyze::keepCurrent(int state)
{
    Q_UNUSED(state);
    if (keepCurrentCB->isChecked())
    {
        removeStatsCursor();
        replot();
    }
}

// Get the following or previous .analyze file from the directory currently being displayed.
QString Analyze::getNextFile(bool after)
{
    QDir dir;
    QString filename;
    QString dirString;
    if (runtimeDisplay)
    {
        // Use the directory and file we're currently writing to.
        dirString = QUrl::fromLocalFile(QDir(KSPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation)).filePath("analyze")).toLocalFile();
        filename = QFileInfo(logFilename).fileName();
    }
    else
    {
        // Use the directory and file we're currently displaying.
        dirString = dirPath.toLocalFile();
        filename = QFileInfo(displayedSession.toLocalFile()).fileName();
    }

    // Set the sorting by name and filter by a .analyze suffix and get the file list.
    dir.setPath(dirString);
    QStringList filters;
    filters << "*.analyze";
    dir.setNameFilters(filters);
    dir.setSorting(QDir::Name);
    QStringList fileList = dir.entryList();

    if (fileList.size() == 0)
        return "";

    // This would be the case on startup in 'Current Session' mode, but it hasn't started up yet.
    if (filename.isEmpty() && fileList.size() > 0 && !after)
        return QFileInfo(dirString, fileList.last()).absoluteFilePath();

    // Go through all the files in this directory and find the file currently being displayed.
    int index = -1;
    for (int i = fileList.size() - 1; i >= 0; --i)
    {
        if (fileList[i] == filename)
        {
            index = i;
            break;
        }
    }

    // Make sure we can go before or after.
    if (index < 0)
        return "";
    else if (!after && index <= 0)
        return "";
    else if (after && index >= fileList.size() - 1)
        return "";

    return QFileInfo(dirString, after ? fileList[index + 1] : fileList[index - 1]).absoluteFilePath();
}

void Analyze::nextFile()
{
    QString filename = getNextFile(true);
    if (filename.isEmpty())
        displayFile(QUrl(), true);
    else
        displayFile(QUrl::fromLocalFile(filename));

}

void Analyze::prevFile()
{
    QString filename = getNextFile(false);
    if (filename.isEmpty())
        return;
    displayFile(QUrl::fromLocalFile(filename));
}

// Do what's necessary to display the .analyze file passed in.
void Analyze::displayFile(const QUrl &url, bool forceCurrentSession)
{
    if (forceCurrentSession || (logFilename.size() > 0 && url.toLocalFile() == logFilename))
    {
        // Input from current session
        inputCombo->setCurrentIndex(0);
        inputValue->setText("");
        if (!runtimeDisplay)
        {
            reset();
            maxXValue = readDataFromFile(logFilename);
        }
        runtimeDisplay = true;
        fullWidthCB->setChecked(true);
        fullWidthCB->setVisible(true);
        fullWidthCB->setDisabled(false);
        displayedSession = QUrl();
        replot();
        return;
    }

    inputCombo->setCurrentIndex(1);
    displayedSession = url;
    dirPath = QUrl(url.url(QUrl::RemoveFilename));

    reset();
    inputValue->setText(url.fileName());

    // If we do this after the readData call below, it would animate the sequence.
    runtimeDisplay = false;

    maxXValue = readDataFromFile(url.toLocalFile());
    checkForMissingSchedulerJobEnd(maxXValue);
    plotStart = 0;
    plotWidth = maxXValue + 5;
    replot();
}

// Implements the input selection UI.
// User can either choose the current Ekos session, or a file read from disk.
void Analyze::initInputSelection()
{
    // Setup the input combo box.
    dirPath = QUrl::fromLocalFile(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("analyze"));

    inputCombo->addItem(i18n("Current Session"));
    inputCombo->addItem(i18n("Read from File"));
    inputValue->setText("");
    inputCombo->setCurrentIndex(0);
    connect(inputCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&](int index)
    {
        if (index == 0)
        {
            displayFile(QUrl::fromLocalFile(logFilename), true);
        }
        else if (index == 1)
        {
            // The i18n call below is broken up (and the word "analyze" is protected from it) because i18n
            // translates "analyze" to "analyse" for the English UK locale, but we need to keep it ".analyze"
            // because that's what how the files are named.
            QUrl inputURL = QFileDialog::getOpenFileUrl(this, i18nc("@title:window", "Select input file"), dirPath,
                            QString("Analyze %1 (*.analyze);;%2").arg(i18n("Log")).arg(i18n("All Files (*)")));
            if (inputURL.isEmpty())
                return;
            displayFile(inputURL);
        }
    });
    connect(nextFileB, &QPushButton::clicked, this, &Analyze::nextFile);
    connect(prevFileB, &QPushButton::clicked, this, &Analyze::prevFile);
}

void Analyze::setupKeyboardShortcuts(QWidget *plot)
{
    // Shortcuts defined: https://doc.qt.io/archives/qt-4.8/qkeysequence.html#standard-shortcuts
    QShortcut *s = new QShortcut(QKeySequence(QKeySequence::ZoomIn), plot);
    connect(s, &QShortcut::activated, this, &Analyze::zoomIn);
    s = new QShortcut(QKeySequence(QKeySequence::ZoomOut), plot);
    connect(s, &QShortcut::activated, this, &Analyze::zoomOut);

    s = new QShortcut(QKeySequence(QKeySequence::MoveToNextChar), plot);
    connect(s, &QShortcut::activated, this, &Analyze::scrollRight);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousChar), plot);
    connect(s, &QShortcut::activated, this, &Analyze::scrollLeft);

    s = new QShortcut(QKeySequence(QKeySequence::MoveToNextWord), plot);
    connect(s, &QShortcut::activated, this, &Analyze::nextTimelineItem);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousWord), plot);
    connect(s, &QShortcut::activated, this, &Analyze::previousTimelineItem);

    s = new QShortcut(QKeySequence(QKeySequence::SelectNextWord), plot);
    connect(s, &QShortcut::activated, this, &Analyze::nextTimelineItem);
    s = new QShortcut(QKeySequence(QKeySequence::SelectPreviousWord), plot);
    connect(s, &QShortcut::activated, this, &Analyze::previousTimelineItem);

    s = new QShortcut(QKeySequence(QKeySequence::MoveToNextLine), plot);
    connect(s, &QShortcut::activated, this, &Analyze::statsYZoomIn);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousLine), plot);
    connect(s, &QShortcut::activated, this, &Analyze::statsYZoomOut);
}

Analyze::~Analyze()
{
    // TODO:
    // We should write out to disk any sessions that haven't terminated
    // (e.g. capture, focus, guide)
}

void Analyze::setSelectedSession(const Session &s)
{
    m_selectedSession = s;
}

void Analyze::clearSelectedSession()
{
    m_selectedSession = Session();
}

// When a user selects a timeline session, the previously selected one
// is deselected.  Note: this does not replot().
void Analyze::unhighlightTimelineItem()
{
    clearSelectedSession();
    if (selectionHighlight != nullptr)
    {
        timelinePlot->removeItem(selectionHighlight);
        selectionHighlight = nullptr;
    }
    detailsTable->clear();
    prevSessionB->setDisabled(true);
    nextSessionB->setDisabled(true);
}

// Highlight the area between start and end of the session on row y in Timeline.
// Note that this doesn't replot().
void Analyze::highlightTimelineItem(const Session &session)
{
    constexpr double halfHeight = 0.5;
    unhighlightTimelineItem();

    setSelectedSession(session);
    QCPItemRect *rect = new QCPItemRect(timelinePlot);
    rect->topLeft->setCoords(session.start, session.offset + halfHeight);
    rect->bottomRight->setCoords(session.end, session.offset - halfHeight);
    rect->setBrush(timelineSelectionBrush);
    selectionHighlight = rect;
    prevSessionB->setDisabled(false);
    nextSessionB->setDisabled(false);

}

// Creates a fat line-segment on the Timeline, optionally with a stripe in the middle.
QCPItemRect * Analyze::addSession(double start, double end, double y,
                                  const QBrush &brush, const QBrush *stripeBrush)
{
    QPen pen = QPen(Qt::black, 1, Qt::SolidLine);
    QCPItemRect *rect = new QCPItemRect(timelinePlot);
    rect->topLeft->setCoords(start, y + halfTimelineHeight);
    rect->bottomRight->setCoords(end, y - halfTimelineHeight);
    rect->setPen(pen);
    rect->setSelectedPen(pen);
    rect->setBrush(brush);
    rect->setSelectedBrush(brush);

    if (stripeBrush != nullptr)
    {
        QCPItemRect *stripe = new QCPItemRect(timelinePlot);
        stripe->topLeft->setCoords(start, y + halfTimelineHeight / 2.0);
        stripe->bottomRight->setCoords(end, y - halfTimelineHeight / 2.0);
        stripe->setPen(pen);
        stripe->setBrush(*stripeBrush);
    }
    return rect;
}

// Add the guide stats values to the Stats graphs.
// We want to avoid drawing guide-stat values when not guiding.
// That is, we have no input samples then, but the graph would connect
// two points with a line. By adding NaN values into the graph,
// those places are made invisible.
void Analyze::addGuideStats(double raDrift, double decDrift, int raPulse, int decPulse, double snr,
                            int numStars, double skyBackground, double time)
{
    double MAX_GUIDE_STATS_GAP = 30;

    if (time - lastGuideStatsTime > MAX_GUIDE_STATS_GAP &&
            lastGuideStatsTime >= 0)
    {
        addGuideStatsInternal(qQNaN(), qQNaN(), 0, 0, qQNaN(), qQNaN(), qQNaN(), qQNaN(), qQNaN(),
                              lastGuideStatsTime + .0001);
        addGuideStatsInternal(qQNaN(), qQNaN(), 0, 0, qQNaN(), qQNaN(), qQNaN(), qQNaN(), qQNaN(), time - .0001);
        guiderRms->resetFilter();
    }

    const double drift = std::hypot(raDrift, decDrift);

    // To compute the RMS error, which is sqrt(sum square error / N), filter the squared
    // error, which effectively returns sum squared error / N, and take the sqrt.
    // This is done by RmsFilter::newSample().
    const double rms = guiderRms->newSample(raDrift, decDrift);
    addGuideStatsInternal(raDrift, decDrift, double(raPulse), double(decPulse), snr, numStars, skyBackground, drift, rms, time);

    // If capture is active, plot the capture RMS.
    if (captureStartedTime >= 0)
    {
        // lastCaptureRmsTime is the last time we plotted a capture RMS value.
        // If we have plotted values previously, and there's a gap in guiding
        // we must place NaN values in the graph surrounding the gap.
        if ((lastCaptureRmsTime >= 0) &&
                (time - lastCaptureRmsTime > MAX_GUIDE_STATS_GAP))
        {
            // this is the first sample in a series with a gap behind us.
            statsPlot->graph(CAPTURE_RMS_GRAPH)->addData(lastCaptureRmsTime + .0001, qQNaN());
            statsPlot->graph(CAPTURE_RMS_GRAPH)->addData(time - .0001, qQNaN());
            captureRms->resetFilter();
        }
        const double rmsC = captureRms->newSample(raDrift, decDrift);
        statsPlot->graph(CAPTURE_RMS_GRAPH)->addData(time, rmsC);
        lastCaptureRmsTime = time;
    }

    lastGuideStatsTime = time;
}

void Analyze::addGuideStatsInternal(double raDrift, double decDrift, double raPulse,
                                    double decPulse, double snr,
                                    double numStars, double skyBackground,
                                    double drift, double rms, double time)
{
    statsPlot->graph(RA_GRAPH)->addData(time, raDrift);
    statsPlot->graph(DEC_GRAPH)->addData(time, decDrift);
    statsPlot->graph(RA_PULSE_GRAPH)->addData(time, raPulse);
    statsPlot->graph(DEC_PULSE_GRAPH)->addData(time, decPulse);
    statsPlot->graph(DRIFT_GRAPH)->addData(time, drift);
    statsPlot->graph(RMS_GRAPH)->addData(time, rms);

    // Set the SNR axis' maximum to 95% of the way up from the middle to the top.
    if (!qIsNaN(snr))
        snrMax = std::max(snr, snrMax);
    if (!qIsNaN(skyBackground))
        skyBgMax = std::max(skyBackground, skyBgMax);
    if (!qIsNaN(numStars))
        numStarsMax = std::max(numStars, static_cast<double>(numStarsMax));

    statsPlot->graph(SNR_GRAPH)->addData(time, snr);
    statsPlot->graph(NUMSTARS_GRAPH)->addData(time, numStars);
    statsPlot->graph(SKYBG_GRAPH)->addData(time, skyBackground);
}

void Analyze::addTemperature(double temperature, double time)
{
    // The HFR corresponds to the last capture
    // If there is no temperature sensor, focus sends a large negative value.
    if (temperature > -200)
        statsPlot->graph(TEMPERATURE_GRAPH)->addData(time, temperature);
}

void Analyze::addFocusPosition(double focusPosition, double time)
{
    statsPlot->graph(FOCUS_POSITION_GRAPH)->addData(time, focusPosition);
}

void Analyze::addTargetDistance(double targetDistance, double time)
{
    // The target distance corresponds to the last capture
    if (previousCaptureStartedTime >= 0 && previousCaptureCompletedTime >= 0 &&
            previousCaptureStartedTime < previousCaptureCompletedTime &&
            previousCaptureCompletedTime <= time)
    {
        statsPlot->graph(TARGET_DISTANCE_GRAPH)->addData(previousCaptureStartedTime - .0001, qQNaN());
        statsPlot->graph(TARGET_DISTANCE_GRAPH)->addData(previousCaptureStartedTime, targetDistance);
        statsPlot->graph(TARGET_DISTANCE_GRAPH)->addData(previousCaptureCompletedTime, targetDistance);
        statsPlot->graph(TARGET_DISTANCE_GRAPH)->addData(previousCaptureCompletedTime + .0001, qQNaN());
    }
}

// Add the HFR values to the Stats graph, as a constant value between startTime and time.
void Analyze::addHFR(double hfr, int numCaptureStars, int median, double eccentricity,
                     double time, double startTime)
{
    // The HFR corresponds to the last capture
    statsPlot->graph(HFR_GRAPH)->addData(startTime - .0001, qQNaN());
    statsPlot->graph(HFR_GRAPH)->addData(startTime, hfr);
    statsPlot->graph(HFR_GRAPH)->addData(time, hfr);
    statsPlot->graph(HFR_GRAPH)->addData(time + .0001, qQNaN());

    statsPlot->graph(NUM_CAPTURE_STARS_GRAPH)->addData(startTime - .0001, qQNaN());
    statsPlot->graph(NUM_CAPTURE_STARS_GRAPH)->addData(startTime, numCaptureStars);
    statsPlot->graph(NUM_CAPTURE_STARS_GRAPH)->addData(time, numCaptureStars);
    statsPlot->graph(NUM_CAPTURE_STARS_GRAPH)->addData(time + .0001, qQNaN());

    statsPlot->graph(MEDIAN_GRAPH)->addData(startTime - .0001, qQNaN());
    statsPlot->graph(MEDIAN_GRAPH)->addData(startTime, median);
    statsPlot->graph(MEDIAN_GRAPH)->addData(time, median);
    statsPlot->graph(MEDIAN_GRAPH)->addData(time + .0001, qQNaN());

    statsPlot->graph(ECCENTRICITY_GRAPH)->addData(startTime - .0001, qQNaN());
    statsPlot->graph(ECCENTRICITY_GRAPH)->addData(startTime, eccentricity);
    statsPlot->graph(ECCENTRICITY_GRAPH)->addData(time, eccentricity);
    statsPlot->graph(ECCENTRICITY_GRAPH)->addData(time + .0001, qQNaN());

    medianMax = std::max(median, medianMax);
    numCaptureStarsMax = std::max(numCaptureStars, numCaptureStarsMax);
}

// Add the Mount Coordinates values to the Stats graph.
// All but pierSide are in double degrees.
void Analyze::addMountCoords(double ra, double dec, double az,
                             double alt, int pierSide, double ha, double time)
{
    statsPlot->graph(MOUNT_RA_GRAPH)->addData(time, ra);
    statsPlot->graph(MOUNT_DEC_GRAPH)->addData(time, dec);
    statsPlot->graph(MOUNT_HA_GRAPH)->addData(time, ha);
    statsPlot->graph(AZ_GRAPH)->addData(time, az);
    statsPlot->graph(ALT_GRAPH)->addData(time, alt);
    statsPlot->graph(PIER_SIDE_GRAPH)->addData(time, double(pierSide));
}

// Read a .analyze file, and setup all the graphics.
double Analyze::readDataFromFile(const QString &filename)
{
    double lastTime = 10;
    QFile inputFile(filename);
    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            double time = processInputLine(line);
            if (time > lastTime)
                lastTime = time;
        }
        inputFile.close();
    }
    return lastTime;
}

// Process an input line read from a .analyze file.
double Analyze::processInputLine(const QString &line)
{
    bool ok;
    // Break the line into comma-separated components
    QStringList list = line.split(QLatin1Char(','));
    // We need at least a command and a timestamp
    if (list.size() < 2)
        return 0;
    if (list[0].at(0).toLatin1() == '#')
    {
        // Comment character # must be at start of line.
        return 0;
    }

    if ((list[0] == "AnalyzeStartTime") && list.size() == 3)
    {
        displayStartTime = QDateTime::fromString(list[1], timeFormat);
        startTimeInitialized = true;
        analyzeTimeZone = list[2];
        return 0;
    }

    // Except for comments and the above AnalyzeStartTime, the second item
    // in the csv line is a double which represents seconds since start of the log.
    const double time = QString(list[1]).toDouble(&ok);
    if (!ok)
        return 0;
    if (time < 0 || time > 3600 * 24 * 10)
        return 0;

    if ((list[0] == "CaptureStarting") && (list.size() == 4))
    {
        const double exposureSeconds = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        const QString filter = list[3];
        processCaptureStarting(time, exposureSeconds, filter);
    }
    else if ((list[0] == "CaptureComplete") && (list.size() >= 6) && (list.size() <= 9))
    {
        const double exposureSeconds = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        const QString filter = list[3];
        const double hfr = QString(list[4]).toDouble(&ok);
        if (!ok)
            return 0;
        const QString filename = list[5];
        const int numStars = (list.size() > 6) ? QString(list[6]).toInt(&ok) : 0;
        if (!ok)
            return 0;
        const int median = (list.size() > 7) ? QString(list[7]).toInt(&ok) : 0;
        if (!ok)
            return 0;
        const double eccentricity = (list.size() > 8) ? QString(list[8]).toDouble(&ok) : 0;
        if (!ok)
            return 0;
        processCaptureComplete(time, filename, exposureSeconds, filter, hfr, numStars, median, eccentricity, true);
    }
    else if ((list[0] == "CaptureAborted") && (list.size() == 3))
    {
        const double exposureSeconds = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        processCaptureAborted(time, exposureSeconds, true);
    }
    else if ((list[0] == "AutofocusStarting") && (list.size() >= 4))
    {
        QString filter = list[2];
        double temperature = QString(list[3]).toDouble(&ok);
        if (!ok)
            return 0;
        AutofocusReason reason;
        QString reasonInfo;
        if (list.size() == 4)
        {
            reason = AutofocusReason::FOCUS_NONE;
            reasonInfo = "";
        }
        else
        {
            reason = static_cast<AutofocusReason>(QString(list[4]).toInt(&ok));
            if (!ok)
                return 0;
            reasonInfo = list[5];
        }
        processAutofocusStarting(time, temperature, filter, reason, reasonInfo);
    }
    else if ((list[0] == "AutofocusComplete") && (list.size() >= 8))
    {
        // Version 2
        double temperature = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        QVariant reasonV = QString(list[3]);
        int reasonInt = reasonV.toInt();
        if (reasonInt < 0 || reasonInt >= AutofocusReason::FOCUS_MAX_REASONS)
            return 0;
        AutofocusReason reason = static_cast<AutofocusReason>(reasonInt);
        const QString reasonInfo = list[4];
        const QString filter = list[5];
        const QString samples = list[6];
        const bool useWeights = QString(list[7]).toInt(&ok);
        if (!ok)
            return 0;
        const QString curve = list.size() > 8 ? list[8] : "";
        const QString title = list.size() > 9 ? list[9] : "";
        processAutofocusCompleteV2(time, temperature, filter, reason, reasonInfo, samples, useWeights, curve, title, true);
    }
    else if ((list[0] == "AutofocusComplete") && (list.size() >= 4))
    {
        // Version 1
        const QString filter = list[2];
        const QString samples = list[3];
        const QString curve = list.size() > 4 ? list[4] : "";
        const QString title = list.size() > 5 ? list[5] : "";
        processAutofocusComplete(time, filter, samples, curve, title, true);
    }
    else if ((list[0] == "AutofocusAborted") && (list.size() >= 9))
    {
        double temperature = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        QVariant reasonV = QString(list[3]);
        int reasonInt = reasonV.toInt();
        if (reasonInt < 0 || reasonInt >= AutofocusReason::FOCUS_MAX_REASONS)
            return 0;
        AutofocusReason reason = static_cast<AutofocusReason>(reasonInt);
        QString reasonInfo = list[4];
        QString filter = list[5];
        QString samples = list[6];
        bool useWeights = QString(list[7]).toInt(&ok);
        if (!ok)
            return 0;
        AutofocusFailReason failCode;
        QVariant failCodeV = QString(list[8]);
        int failCodeInt = failCodeV.toInt();
        if (failCodeInt < 0 || failCodeInt >= AutofocusFailReason::FOCUS_FAIL_MAX_REASONS)
            return 0;
        failCode = static_cast<AutofocusFailReason>(failCodeInt);
        if (!ok)
            return 0;
        QString failCodeInfo;
        if (list.size() > 9)
            failCodeInfo = QString(list[9]);
        processAutofocusAbortedV2(time, temperature, filter, reason, reasonInfo, samples, useWeights, failCode, failCodeInfo, true);
    }
    else if ((list[0] == "AutofocusAborted") && (list.size() >= 4))
    {
        QString filter = list[2];
        QString samples = list[3];
        processAutofocusAborted(time, filter, samples, true);
    }
    else if ((list[0] == "AdaptiveFocusComplete") && (list.size() == 12))
    {
        // This is the second version of the AdaptiveFocusComplete message
        const QString filter = list[2];
        double temperature = QString(list[3]).toDouble(&ok);
        const double tempTicks = QString(list[4]).toDouble(&ok);
        double altitude = QString(list[5]).toDouble(&ok);
        const double altTicks = QString(list[6]).toDouble(&ok);
        const int prevPosError = QString(list[7]).toInt(&ok);
        const int thisPosError = QString(list[8]).toInt(&ok);
        const int totalTicks = QString(list[9]).toInt(&ok);
        const int position = QString(list[10]).toInt(&ok);
        const bool focuserMoved = QString(list[11]).toInt(&ok) != 0;
        processAdaptiveFocusComplete(time, filter, temperature, tempTicks, altitude, altTicks, prevPosError,
                                     thisPosError, totalTicks, position, focuserMoved, true);
    }
    else if ((list[0] == "AdaptiveFocusComplete") && (list.size() >= 9))
    {
        // This is the first version of the AdaptiveFocusComplete message - retained os Analyze can process
        // historical messages correctly
        const QString filter = list[2];
        double temperature = QString(list[3]).toDouble(&ok);
        const int tempTicks = QString(list[4]).toInt(&ok);
        double altitude = QString(list[5]).toDouble(&ok);
        const int altTicks = QString(list[6]).toInt(&ok);
        const int totalTicks = QString(list[7]).toInt(&ok);
        const int position = QString(list[8]).toInt(&ok);
        const bool focuserMoved = list.size() < 10 || QString(list[9]).toInt(&ok) != 0;
        processAdaptiveFocusComplete(time, filter, temperature, tempTicks,
                                     altitude, altTicks, 0, 0, totalTicks, position, focuserMoved, true);
    }
    else if ((list[0] == "GuideState") && list.size() == 3)
    {
        processGuideState(time, list[2], true);
    }
    else if ((list[0] == "GuideStats") && list.size() == 9)
    {
        const double ra = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        const double dec = QString(list[3]).toDouble(&ok);
        if (!ok)
            return 0;
        const double raPulse = QString(list[4]).toInt(&ok);
        if (!ok)
            return 0;
        const double decPulse = QString(list[5]).toInt(&ok);
        if (!ok)
            return 0;
        const double snr = QString(list[6]).toDouble(&ok);
        if (!ok)
            return 0;
        const double skyBg = QString(list[7]).toDouble(&ok);
        if (!ok)
            return 0;
        const double numStars = QString(list[8]).toInt(&ok);
        if (!ok)
            return 0;
        processGuideStats(time, ra, dec, raPulse, decPulse, snr, skyBg, numStars, true);
    }
    else if ((list[0] == "Temperature") && list.size() == 3)
    {
        const double temperature = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        processTemperature(time, temperature, true);
    }
    else if ((list[0] == "TargetDistance") && list.size() == 3)
    {
        const double targetDistance = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        processTargetDistance(time, targetDistance, true);
    }
    else if ((list[0] == "MountState") && list.size() == 3)
    {
        processMountState(time, list[2], true);
    }
    else if ((list[0] == "MountCoords") && (list.size() == 7 || list.size() == 8))
    {
        const double ra = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        const double dec = QString(list[3]).toDouble(&ok);
        if (!ok)
            return 0;
        const double az = QString(list[4]).toDouble(&ok);
        if (!ok)
            return 0;
        const double alt = QString(list[5]).toDouble(&ok);
        if (!ok)
            return 0;
        const int side = QString(list[6]).toInt(&ok);
        if (!ok)
            return 0;
        const double ha = (list.size() > 7) ? QString(list[7]).toDouble(&ok) : 0;
        if (!ok)
            return 0;
        processMountCoords(time, ra, dec, az, alt, side, ha, true);
    }
    else if ((list[0] == "AlignState") && list.size() == 3)
    {
        processAlignState(time, list[2], true);
    }
    else if ((list[0] == "MeridianFlipState") && list.size() == 3)
    {
        processMountFlipState(time, list[2], true);
    }
    else if ((list[0] == "SchedulerJobStart") && list.size() == 3)
    {
        QString jobName = list[2];
        processSchedulerJobStarted(time, jobName);
    }
    else if ((list[0] == "SchedulerJobEnd") && list.size() == 4)
    {
        QString jobName = list[2];
        QString reason = list[3];
        processSchedulerJobEnded(time, jobName, reason, true);
    }
    else
    {
        return 0;
    }
    return time;
}

namespace
{
void addDetailsRow(QTableWidget *table, const QString &col1, const QColor &color1,
                   const QString &col2, const QColor &color2,
                   const QString &col3 = "", const QColor &color3 = Qt::white)
{
    int row = table->rowCount();
    table->setRowCount(row + 1);

    QTableWidgetItem *item = new QTableWidgetItem();
    if (col1 == "Filename")
    {
        // Special case filenames--they tend to be too long and get elided.
        QFont ft = item->font();
        ft.setPointSizeF(8.0);
        item->setFont(ft);
        item->setText(col2);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setForeground(color2);
        table->setItem(row, 0, item);
        table->setSpan(row, 0, 1, 3);
        return;
    }

    item->setText(col1);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item->setForeground(color1);
    table->setItem(row, 0, item);

    item = new QTableWidgetItem();
    item->setText(col2);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item->setForeground(color2);
    if (col1 == "Filename")
    {
        // Special Case long filenames.
        QFont ft = item->font();
        ft.setPointSizeF(8.0);
        item->setFont(ft);
    }
    table->setItem(row, 1, item);

    if (col3.size() > 0)
    {
        item = new QTableWidgetItem();
        item->setText(col3);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setForeground(color3);
        table->setItem(row, 2, item);
    }
    else
    {
        // Column 1 spans 2nd and 3rd columns
        table->setSpan(row, 1, 1, 2);
    }
}
}

// Helper to create tables in the details display.
// Start the table, displaying the heading and timing information, common to all sessions.
void Analyze::Session::setupTable(const QString &name, const QString &status,
                                  const QDateTime &startClock, const QDateTime &endClock, QTableWidget *table)
{
    details = table;
    details->clear();
    details->setRowCount(0);
    details->setEditTriggers(QAbstractItemView::NoEditTriggers);
    details->setColumnCount(3);
    details->verticalHeader()->setDefaultSectionSize(20);
    details->horizontalHeader()->setStretchLastSection(true);
    details->setColumnWidth(0, 100);
    details->setColumnWidth(1, 100);
    details->setShowGrid(false);
    details->setWordWrap(true);
    details->horizontalHeader()->hide();
    details->verticalHeader()->hide();

    QString startDateStr = startClock.toString("dd.MM.yyyy");
    QString startTimeStr = startClock.toString("hh:mm:ss");
    QString endTimeStr = isTemporary() ? "Ongoing"
                         : endClock.toString("hh:mm:ss");

    addDetailsRow(details, name, Qt::yellow, status, Qt::yellow);
    addDetailsRow(details, "Date", Qt::yellow, startDateStr, Qt::white);
    addDetailsRow(details, "Interval", Qt::yellow, QString::number(start, 'f', 3), Qt::white,
                  isTemporary() ? "Ongoing" : QString::number(end, 'f', 3), Qt::white);
    addDetailsRow(details, "Clock", Qt::yellow, startTimeStr, Qt::white, endTimeStr, Qt::white);
    addDetailsRow(details, "Duration", Qt::yellow, QString::number(end - start, 'f', 1), Qt::white);
}

// Add a new row to the table, which is specific to the particular Timeline line.
void Analyze::Session::addRow(const QString &key, const QString &value)
{
    addDetailsRow(details, key, Qt::yellow, value, Qt::white);
}

bool Analyze::Session::isTemporary() const
{
    return rect != nullptr;
}

// This is version 2 of FocusSession that includes weights, outliers and reason codes
// The focus session parses the "pipe-separate-values" list of positions
// and HFRs given it, eventually to be used to plot the focus v-curve.
Analyze::FocusSession::FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                                    const QString &filter_, const AutofocusReason reason_, const QString &reasonInfo_, const QString &points_,
                                    const bool useWeights_, const QString &curve_, const QString &title_, const AutofocusFailReason failCode_,
                                    const QString failCodeInfo_)
    : Session(start_, end_, FOCUS_Y, rect), success(ok), temperature(temperature_), filter(filter_), reason(reason_),
      reasonInfo(reasonInfo_), points(points_), useWeights(useWeights_), curve(curve_), title(title_), failCode(failCode_),
      failCodeInfo(failCodeInfo_)
{
    const QStringList list = points.split(QLatin1Char('|'));
    const int size = list.size();
    // Size can be 1 if points_ is an empty string.
    if (size < 2)
        return;

    for (int i = 0; i < size; )
    {
        bool parsed1, parsed2, parsed3, parsed4;
        int position = QString(list[i++]).toInt(&parsed1);
        if (i >= size)
            break;
        double hfr = QString(list[i++]).toDouble(&parsed2);
        double weight = QString(list[i++]).toDouble(&parsed3);
        bool outlier = QString(list[i++]).toInt(&parsed4);
        if (!parsed1 || !parsed2 || !parsed3 || !parsed4)
        {
            positions.clear();
            hfrs.clear();
            weights.clear();
            outliers.clear();
            return;
        }
        positions.push_back(position);
        hfrs.push_back(hfr);
        weights.push_back(weight);
        outliers.push_back(outlier);
    }
}

// This is the original version of FocusSession
// The focus session parses the "pipe-separate-values" list of positions
// and HFRs given it, eventually to be used to plot the focus v-curve.
Analyze::FocusSession::FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                                    const QString &filter_, const QString &points_, const QString &curve_, const QString &title_)
    : Session(start_, end_, FOCUS_Y, rect), success(ok),
      temperature(temperature_), filter(filter_), points(points_), curve(curve_), title(title_)
{
    // Set newer variables, not part of the original message, to default values
    reason = AutofocusReason::FOCUS_NONE;
    reasonInfo = "";
    useWeights = false;
    failCode = AutofocusFailReason::FOCUS_FAIL_NONE;
    failCodeInfo = "";

    const QStringList list = points.split(QLatin1Char('|'));
    const int size = list.size();
    // Size can be 1 if points_ is an empty string.
    if (size < 2)
        return;

    for (int i = 0; i < size; )
    {
        bool parsed1, parsed2;
        int position = QString(list[i++]).toInt(&parsed1);
        if (i >= size)
            break;
        double hfr = QString(list[i++]).toDouble(&parsed2);
        if (!parsed1 || !parsed2)
        {
            positions.clear();
            hfrs.clear();
            weights.clear();
            outliers.clear();
            return;
        }
        positions.push_back(position);
        hfrs.push_back(hfr);
        weights.push_back(1.0);
        outliers.push_back(false);
    }
}

Analyze::FocusSession::FocusSession(double start_, double end_, QCPItemRect *rect,
                                    const QString &filter_, double temperature_, double tempTicks_, double altitude_,
                                    double altTicks_, int prevPosError_, int thisPosError_, int totalTicks_, int position_)
    : Session(start_, end_, FOCUS_Y, rect), temperature(temperature_), filter(filter_), tempTicks(tempTicks_),
      altitude(altitude_), altTicks(altTicks_), prevPosError(prevPosError_), thisPosError(thisPosError_),
      totalTicks(totalTicks_), adaptedPosition(position_)
{
    standardSession = false;
}

double Analyze::FocusSession::focusPosition()
{
    if (!standardSession)
        return adaptedPosition;

    if (positions.size() > 0)
        return positions.last();
    return 0;
}

namespace
{
bool isTemporaryFile(const QString &filename)
{
    QString tempFileLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return filename.startsWith(tempFileLocation);
}
}

// When the user clicks on a particular capture session in the timeline,
// a table is rendered in the details section, and, if it was a double click,
// the fits file is displayed, if it can be found.
void Analyze::captureSessionClicked(CaptureSession &c, bool doubleClick)
{
    highlightTimelineItem(c);

    if (c.isTemporary())
        c.setupTable("Capture", "in progress", clockTime(c.start), clockTime(c.start), detailsTable);
    else if (c.aborted)
        c.setupTable("Capture", "ABORTED", clockTime(c.start), clockTime(c.end), detailsTable);
    else
        c.setupTable("Capture", "successful", clockTime(c.start), clockTime(c.end), detailsTable);

    c.addRow("Filter", c.filter);

    double raRMS, decRMS, totalRMS;
    int numSamples;
    displayGuideGraphics(c.start, c.end, &raRMS, &decRMS, &totalRMS, &numSamples);
    if (numSamples > 0)
        c.addRow("GuideRMS", QString::number(totalRMS, 'f', 2));

    c.addRow("Exposure", QString::number(c.duration, 'f', 2));
    if (!c.isTemporary())
        c.addRow("Filename", c.filename);


    // Don't try to display images from temporary sessions (they aren't done yet).
    if (doubleClick && !c.isTemporary())
    {
        QString filename = findFilename(c.filename);
        // Don't display temporary files from completed sessions either.
        bool tempImage = isTemporaryFile(c.filename);
        if (!tempImage && filename.size() == 0)
            appendLogText(i18n("Could not find image file: %1", c.filename));
        else if (!tempImage)
            displayFITS(filename);
        else appendLogText(i18n("Cannot display temporary image file: %1", c.filename));
    }
}

namespace
{
QString getSign(int val)
{
    if (val == 0) return "";
    else if (val > 0) return "+";
    else return "-";
}
QString signedIntString(int val)
{
    return QString("%1%2").arg(getSign(val)).arg(abs(val));
}
}


// When the user clicks on a focus session in the timeline,
// a table is rendered in the details section, and the HFR/position plot
// is displayed in the graphics plot. If focus is ongoing
// the information for the graphics is not plotted as it is not yet available.
void Analyze::focusSessionClicked(FocusSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c);

    if (!c.standardSession)
    {
        // This is an adaptive focus session
        c.setupTable("Focus", "Adaptive", clockTime(c.end), clockTime(c.end), detailsTable);
        c.addRow("Filter", c.filter);
        addDetailsRow(detailsTable, "Temperature", Qt::yellow, QString("%1°").arg(c.temperature, 0, 'f', 1),
                      Qt::white, QString("%1").arg(c.tempTicks, 0, 'f', 1));
        addDetailsRow(detailsTable, "Altitude", Qt::yellow, QString("%1°").arg(c.altitude, 0, 'f', 1),
                      Qt::white, QString("%1").arg(c.altTicks, 0, 'f', 1));
        addDetailsRow(detailsTable, "Pos Error", Qt::yellow, "Start / End", Qt::white,
                      QString("%1 / %2").arg(c.prevPosError).arg(c.thisPosError));
        addDetailsRow(detailsTable, "Position", Qt::yellow, QString::number(c.adaptedPosition),
                      Qt::white, signedIntString(c.totalTicks));
        return;
    }

    if (c.success)
        c.setupTable("Focus", "successful", clockTime(c.start), clockTime(c.end), detailsTable);
    else if (c.isTemporary())
        c.setupTable("Focus", "in progress", clockTime(c.start), clockTime(c.start), detailsTable);
    else
        c.setupTable("Focus", "FAILED", clockTime(c.start), clockTime(c.end), detailsTable);

    if (!c.isTemporary())
    {
        if (c.success)
        {
            if (c.hfrs.size() > 0)
                c.addRow("HFR", QString::number(c.hfrs.last(), 'f', 2));
            if (c.positions.size() > 0)
                c.addRow("Solution", QString::number(c.positions.last(), 'f', 0));
        }
        c.addRow("Iterations", QString::number(c.positions.size()));
    }
    addDetailsRow(detailsTable, "Reason", Qt::yellow, AutofocusReasonStr[c.reason], Qt::white, c.reasonInfo, Qt::white);
    if (!c.success && !c.isTemporary())
        addDetailsRow(detailsTable, "Fail Reason", Qt::yellow, AutofocusFailReasonStr[c.failCode], Qt::white, c.failCodeInfo,
                      Qt::white);

    c.addRow("Filter", c.filter);
    c.addRow("Temperature", (c.temperature == INVALID_VALUE) ? "N/A" : QString::number(c.temperature, 'f', 1));

    if (c.isTemporary())
        resetGraphicsPlot();
    else
        displayFocusGraphics(c.positions, c.hfrs, c.useWeights, c.weights, c.outliers, c.curve, c.title, c.success);
}

// When the user clicks on a guide session in the timeline,
// a table is rendered in the details section. If it has a G_GUIDING state
// then a drift plot is generated and  RMS values are calculated
// for the guiding session's time interval.
void Analyze::guideSessionClicked(GuideSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c);

    QString st;
    if (c.simpleState == G_IDLE)
        st = "Idle";
    else if (c.simpleState == G_GUIDING)
        st = "Guiding";
    else if (c.simpleState == G_CALIBRATING)
        st = "Calibrating";
    else if (c.simpleState == G_SUSPENDED)
        st = "Suspended";
    else if (c.simpleState == G_DITHERING)
        st = "Dithering";

    c.setupTable("Guide", st, clockTime(c.start), clockTime(c.end), detailsTable);
    resetGraphicsPlot();
    if (c.simpleState == G_GUIDING)
    {
        double raRMS, decRMS, totalRMS;
        int numSamples;
        displayGuideGraphics(c.start, c.end, &raRMS, &decRMS, &totalRMS, &numSamples);
        if (numSamples > 0)
        {
            c.addRow("total RMS", QString::number(totalRMS, 'f', 2));
            c.addRow("ra RMS", QString::number(raRMS, 'f', 2));
            c.addRow("dec RMS", QString::number(decRMS, 'f', 2));
        }
        c.addRow("Num Samples", QString::number(numSamples));
    }
}

void Analyze::displayGuideGraphics(double start, double end, double *raRMS,
                                   double *decRMS, double *totalRMS, int *numSamples)
{
    resetGraphicsPlot();
    auto ra = statsPlot->graph(RA_GRAPH)->data()->findBegin(start);
    auto dec = statsPlot->graph(DEC_GRAPH)->data()->findBegin(start);
    auto raEnd = statsPlot->graph(RA_GRAPH)->data()->findEnd(end);
    auto decEnd = statsPlot->graph(DEC_GRAPH)->data()->findEnd(end);
    int num = 0;
    double raSquareErrorSum = 0, decSquareErrorSum = 0;
    while (ra != raEnd && dec != decEnd &&
            ra->mainKey() < end && dec->mainKey() < end &&
            ra != statsPlot->graph(RA_GRAPH)->data()->constEnd() &&
            dec != statsPlot->graph(DEC_GRAPH)->data()->constEnd() &&
            ra->mainKey() < end && dec->mainKey() < end)
    {
        const double raVal = ra->mainValue();
        const double decVal = dec->mainValue();
        graphicsPlot->graph(GUIDER_GRAPHICS)->addData(raVal, decVal);
        if (!qIsNaN(raVal) && !qIsNaN(decVal))
        {
            raSquareErrorSum += raVal * raVal;
            decSquareErrorSum += decVal * decVal;
            num++;
        }
        ra++;
        dec++;
    }
    if (numSamples != nullptr)
        *numSamples = num;
    if (num > 0)
    {
        if (raRMS != nullptr)
            *raRMS = sqrt(raSquareErrorSum / num);
        if (decRMS != nullptr)
            *decRMS = sqrt(decSquareErrorSum / num);
        if (totalRMS != nullptr)
            *totalRMS = sqrt((raSquareErrorSum + decSquareErrorSum) / num);
        if (numSamples != nullptr)
            *numSamples = num;
    }
    QCPItemEllipse *c1 = new QCPItemEllipse(graphicsPlot);
    c1->bottomRight->setCoords(1.0, -1.0);
    c1->topLeft->setCoords(-1.0, 1.0);
    QCPItemEllipse *c2 = new QCPItemEllipse(graphicsPlot);
    c2->bottomRight->setCoords(2.0, -2.0);
    c2->topLeft->setCoords(-2.0, 2.0);
    c1->setPen(QPen(Qt::green));
    c2->setPen(QPen(Qt::yellow));

    // Since the plot is wider than it is tall, these lines set the
    // vertical range to 2.5, and the horizontal range to whatever it
    // takes to keep the two axes' scales (number of pixels per value)
    // the same, so that circles stay circular (i.e. circles are not stretch
    // wide even though the graph area is not square).
    graphicsPlot->xAxis->setRange(-2.5, 2.5);
    graphicsPlot->yAxis->setRange(-2.5, 2.5);
    graphicsPlot->xAxis->setScaleRatio(graphicsPlot->yAxis);
}

// When the user clicks on a particular mount session in the timeline,
// a table is rendered in the details section.
void Analyze::mountSessionClicked(MountSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c);

    c.setupTable("Mount", mountStatusString(c.state), clockTime(c.start),
                 clockTime(c.isTemporary() ? c.start : c.end), detailsTable);
}

// When the user clicks on a particular align session in the timeline,
// a table is rendered in the details section.
void Analyze::alignSessionClicked(AlignSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c);
    c.setupTable("Align", getAlignStatusString(c.state), clockTime(c.start),
                 clockTime(c.isTemporary() ? c.start : c.end), detailsTable);
}

// When the user clicks on a particular meridian flip session in the timeline,
// a table is rendered in the details section.
void Analyze::mountFlipSessionClicked(MountFlipSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c);
    c.setupTable("Meridian Flip", MeridianFlipState::meridianFlipStatusString(c.state),
                 clockTime(c.start), clockTime(c.isTemporary() ? c.start : c.end), detailsTable);
}

// When the user clicks on a particular scheduler session in the timeline,
// a table is rendered in the details section.
void Analyze::schedulerSessionClicked(SchedulerJobSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c);
    c.setupTable("Scheduler Job", c.jobName,
                 clockTime(c.start), clockTime(c.isTemporary() ? c.start : c.end), detailsTable);
    c.addRow("End reason", c.reason);
}

// This method determines which timeline session (if any) was selected
// when the user clicks in the Timeline plot. It also sets a cursor
// in the stats plot.
void Analyze::processTimelineClick(QMouseEvent *event, bool doubleClick)
{
    unhighlightTimelineItem();
    double xval = timelinePlot->xAxis->pixelToCoord(event->x());
    double yval = timelinePlot->yAxis->pixelToCoord(event->y());
    if (yval >= CAPTURE_Y - 0.5 && yval <= CAPTURE_Y + 0.5)
    {
        QList<CaptureSession> candidates = captureSessions.find(xval);
        if (candidates.size() > 0)
            captureSessionClicked(candidates[0], doubleClick);
        else if ((temporaryCaptureSession.rect != nullptr) &&
                 (xval > temporaryCaptureSession.start))
            captureSessionClicked(temporaryCaptureSession, doubleClick);
    }
    else if (yval >= FOCUS_Y - 0.5 && yval <= FOCUS_Y + 0.5)
    {
        QList<FocusSession> candidates = focusSessions.find(xval);
        if (candidates.size() > 0)
            focusSessionClicked(candidates[0], doubleClick);
        else if ((temporaryFocusSession.rect != nullptr) &&
                 (xval > temporaryFocusSession.start))
            focusSessionClicked(temporaryFocusSession, doubleClick);
    }
    else if (yval >= GUIDE_Y - 0.5 && yval <= GUIDE_Y + 0.5)
    {
        QList<GuideSession> candidates = guideSessions.find(xval);
        if (candidates.size() > 0)
            guideSessionClicked(candidates[0], doubleClick);
        else if ((temporaryGuideSession.rect != nullptr) &&
                 (xval > temporaryGuideSession.start))
            guideSessionClicked(temporaryGuideSession, doubleClick);
    }
    else if (yval >= MOUNT_Y - 0.5 && yval <= MOUNT_Y + 0.5)
    {
        QList<MountSession> candidates = mountSessions.find(xval);
        if (candidates.size() > 0)
            mountSessionClicked(candidates[0], doubleClick);
        else if ((temporaryMountSession.rect != nullptr) &&
                 (xval > temporaryMountSession.start))
            mountSessionClicked(temporaryMountSession, doubleClick);
    }
    else if (yval >= ALIGN_Y - 0.5 && yval <= ALIGN_Y + 0.5)
    {
        QList<AlignSession> candidates = alignSessions.find(xval);
        if (candidates.size() > 0)
            alignSessionClicked(candidates[0], doubleClick);
        else if ((temporaryAlignSession.rect != nullptr) &&
                 (xval > temporaryAlignSession.start))
            alignSessionClicked(temporaryAlignSession, doubleClick);
    }
    else if (yval >= MERIDIAN_MOUNT_FLIP_Y - 0.5 && yval <= MERIDIAN_MOUNT_FLIP_Y + 0.5)
    {
        QList<MountFlipSession> candidates = mountFlipSessions.find(xval);
        if (candidates.size() > 0)
            mountFlipSessionClicked(candidates[0], doubleClick);
        else if ((temporaryMountFlipSession.rect != nullptr) &&
                 (xval > temporaryMountFlipSession.start))
            mountFlipSessionClicked(temporaryMountFlipSession, doubleClick);
    }
    else if (yval >= SCHEDULER_Y - 0.5 && yval <= SCHEDULER_Y + 0.5)
    {
        QList<SchedulerJobSession> candidates = schedulerJobSessions.find(xval);
        if (candidates.size() > 0)
            schedulerSessionClicked(candidates[0], doubleClick);
        else if ((temporarySchedulerJobSession.rect != nullptr) &&
                 (xval > temporarySchedulerJobSession.start))
            schedulerSessionClicked(temporarySchedulerJobSession, doubleClick);
    }
    setStatsCursor(xval);
    replot();
}

void Analyze::nextTimelineItem()
{
    changeTimelineItem(true);
}

void Analyze::previousTimelineItem()
{
    changeTimelineItem(false);
}

void Analyze::changeTimelineItem(bool next)
{
    if (m_selectedSession.start == 0 && m_selectedSession.end == 0) return;
    switch(m_selectedSession.offset)
    {
        case CAPTURE_Y:
        {
            auto nextSession = next ? captureSessions.findNext(m_selectedSession.start)
                               : captureSessions.findPrevious(m_selectedSession.start);

            // Since we're displaying the images, don't want to stop at an aborted capture.
            // Continue searching until a good session (or no session) is found.
            while (nextSession && nextSession->aborted)
                nextSession = next ? captureSessions.findNext(nextSession->start)
                              : captureSessions.findPrevious(nextSession->start);

            if (nextSession)
            {
                // True because we want to display the image (so simulate a double-click on that session).
                captureSessionClicked(*nextSession, true);
                setStatsCursor((nextSession->end + nextSession->start) / 2);
            }
            break;
        }
        case FOCUS_Y:
        {
            auto nextSession = next ? focusSessions.findNext(m_selectedSession.start)
                               : focusSessions.findPrevious(m_selectedSession.start);
            if (nextSession)
            {
                focusSessionClicked(*nextSession, true);
                setStatsCursor((nextSession->end + nextSession->start) / 2);
            }
            break;
        }
        case ALIGN_Y:
        {
            auto nextSession = next ? alignSessions.findNext(m_selectedSession.start)
                               : alignSessions.findPrevious(m_selectedSession.start);
            if (nextSession)
            {
                alignSessionClicked(*nextSession, true);
                setStatsCursor((nextSession->end + nextSession->start) / 2);
            }
            break;
        }
        case GUIDE_Y:
        {
            auto nextSession = next ? guideSessions.findNext(m_selectedSession.start)
                               : guideSessions.findPrevious(m_selectedSession.start);
            if (nextSession)
            {
                guideSessionClicked(*nextSession, true);
                setStatsCursor((nextSession->end + nextSession->start) / 2);
            }
            break;
        }
        case MOUNT_Y:
        {
            auto nextSession = next ? mountSessions.findNext(m_selectedSession.start)
                               : mountSessions.findPrevious(m_selectedSession.start);
            if (nextSession)
            {
                mountSessionClicked(*nextSession, true);
                setStatsCursor((nextSession->end + nextSession->start) / 2);
            }
            break;
        }
        case SCHEDULER_Y:
        {
            auto nextSession = next ? schedulerJobSessions.findNext(m_selectedSession.start)
                               : schedulerJobSessions.findPrevious(m_selectedSession.start);
            if (nextSession)
            {
                schedulerSessionClicked(*nextSession, true);
                setStatsCursor((nextSession->end + nextSession->start) / 2);
            }
            break;
        }
            //case MERIDIAN_MOUNT_FLIP_Y:
    }
    if (!isVisible(m_selectedSession) && !isVisible(m_selectedSession))
        adjustView((m_selectedSession.start + m_selectedSession.end) / 2.0);
    replot();
}

bool Analyze::isVisible(const Session &s) const
{
    if (fullWidthCB->isChecked())
        return true;
    return !((s.start < plotStart && s.end < plotStart) ||
             (s.start > (plotStart + plotWidth) && s.end > (plotStart + plotWidth)));
}

void Analyze::adjustView(double time)
{
    if (!fullWidthCB->isChecked())
    {
        plotStart = time - plotWidth / 2;
    }
}

void Analyze::setStatsCursor(double time)
{
    removeStatsCursor();

    // Cursor on the stats graph.
    QCPItemLine *line = new QCPItemLine(statsPlot);
    line->setPen(QPen(Qt::darkGray, 1, Qt::SolidLine));
    const double top = statsPlot->yAxis->range().upper;
    const double bottom = statsPlot->yAxis->range().lower;
    line->start->setCoords(time, bottom);
    line->end->setCoords(time, top);
    statsCursor = line;

    // Cursor on the timeline.
    QCPItemLine *line2 = new QCPItemLine(timelinePlot);
    line2->setPen(QPen(Qt::darkGray, 1, Qt::SolidLine));
    const double top2 = timelinePlot->yAxis->range().upper;
    const double bottom2 = timelinePlot->yAxis->range().lower;
    line2->start->setCoords(time, bottom2);
    line2->end->setCoords(time, top2);
    timelineCursor = line2;

    cursorTimeOut->setText(QString("%1s").arg(time));
    cursorClockTimeOut->setText(QString("%1")
                                .arg(clockTime(time).toString("hh:mm:ss")));
    statsCursorTime = time;
    keepCurrentCB->setCheckState(Qt::Unchecked);
}

void Analyze::removeStatsCursor()
{
    if (statsCursor != nullptr)
        statsPlot->removeItem(statsCursor);
    statsCursor = nullptr;

    if (timelineCursor != nullptr)
        timelinePlot->removeItem(timelineCursor);
    timelineCursor = nullptr;

    cursorTimeOut->setText("");
    cursorClockTimeOut->setText("");
    statsCursorTime = -1;
}

// When the users clicks in the stats plot, the cursor is set at the corresponding time.
void Analyze::processStatsClick(QMouseEvent *event, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    double xval = statsPlot->xAxis->pixelToCoord(event->x());
    setStatsCursor(xval);
    replot();
}

void Analyze::timelineMousePress(QMouseEvent *event)
{
    processTimelineClick(event, false);
}

void Analyze::timelineMouseDoubleClick(QMouseEvent *event)
{
    processTimelineClick(event, true);
}

void Analyze::statsMousePress(QMouseEvent *event)
{
    QCPAxis *yAxis = activeYAxis;
    if (!yAxis) return;

    // If we're on the y-axis, adjust the y-axis.
    if (statsPlot->xAxis->pixelToCoord(event->x()) < plotStart)
    {
        yAxisInitialPos = yAxis->pixelToCoord(event->y());
        return;
    }
    processStatsClick(event, false);
}

void Analyze::statsMouseDoubleClick(QMouseEvent *event)
{
    processStatsClick(event, true);
}

// Allow the user to click and hold, causing the cursor to move in real-time.
void Analyze::statsMouseMove(QMouseEvent *event)
{
    QCPAxis *yAxis = activeYAxis;
    if (!yAxis) return;

    // If we're on the y-axis, adjust the y-axis.
    if (statsPlot->xAxis->pixelToCoord(event->x()) < plotStart)
    {
        auto range = yAxis->range();
        double yDiff = yAxisInitialPos - yAxis->pixelToCoord(event->y());
        yAxis->setRange(range.lower + yDiff, range.upper + yDiff);
        replot();
        if (m_YAxisTool.isVisible() && m_YAxisTool.getAxis() == yAxis)
            m_YAxisTool.replot(true);
        return;
    }
    processStatsClick(event, false);
}

// Called by the scrollbar, to move the current view.
void Analyze::scroll(int value)
{
    double pct = static_cast<double>(value) / MAX_SCROLL_VALUE;
    plotStart = std::max(0.0, maxXValue * pct - plotWidth / 2.0);
    // Normally replot adjusts the position of the slider.
    // If the user has done that, we don't want replot to re-do it.
    replot(false);

}
void Analyze::scrollRight()
{
    plotStart = std::min(maxXValue - plotWidth / 5, plotStart + plotWidth / 5);
    fullWidthCB->setChecked(false);
    replot();

}
void Analyze::scrollLeft()
{
    plotStart = std::max(0.0, plotStart - plotWidth / 5);
    fullWidthCB->setChecked(false);
    replot();

}
void Analyze::replot(bool adjustSlider)
{
    adjustTemporarySessions();
    if (fullWidthCB->isChecked())
    {
        plotStart = 0;
        plotWidth = std::max(10.0, maxXValue);
    }
    else if (keepCurrentCB->isChecked())
    {
        plotStart = std::max(0.0, maxXValue - plotWidth);
    }
    // If we're keeping to the latest values,
    // set the time display to the latest time.
    if (keepCurrentCB->isChecked() && statsCursor == nullptr)
    {
        cursorTimeOut->setText(QString("%1s").arg(maxXValue));
        cursorClockTimeOut->setText(QString("%1")
                                    .arg(clockTime(maxXValue).toString("hh:mm:ss")));
    }
    analyzeSB->setPageStep(
        std::min(MAX_SCROLL_VALUE,
                 static_cast<int>(MAX_SCROLL_VALUE * plotWidth / maxXValue)));
    if (adjustSlider)
    {
        double sliderCenter = plotStart + plotWidth / 2.0;
        analyzeSB->setSliderPosition(MAX_SCROLL_VALUE * (sliderCenter / maxXValue));
    }

    timelinePlot->xAxis->setRange(plotStart, plotStart + plotWidth);
    timelinePlot->yAxis->setRange(0, LAST_Y);

    statsPlot->xAxis->setRange(plotStart, plotStart + plotWidth);

    // Rescale any automatic y-axes.
    if (statsPlot->isVisible())
    {
        for (auto &pairs : yAxisMap)
        {
            const YAxisInfo &info = pairs.second;
            if (statsPlot->graph(info.graphIndex)->visible() && info.rescale)
            {
                QCPAxis *axis = info.axis;
                axis->rescale();
                axis->scaleRange(1.1, axis->range().center());
            }
        }
    }

    dateTicker->setOffset(displayStartTime.toMSecsSinceEpoch() / 1000.0);

    timelinePlot->replot();
    statsPlot->replot();
    graphicsPlot->replot();

    if (activeYAxis != nullptr)
    {
        // Adjust the statsPlot padding to align statsPlot and timelinePlot.
        const int widthDiff = statsPlot->axisRect()->width() - timelinePlot->axisRect()->width();
        const int paddingSize = activeYAxis->padding();
        constexpr int maxPadding = 100;
        // Don't quite following why a positive difference should INCREASE padding, but it works.
        const int newPad = std::min(maxPadding, std::max(0, paddingSize + widthDiff));
        if (newPad != paddingSize)
        {
            activeYAxis->setPadding(newPad);
            statsPlot->replot();
        }
    }
    updateStatsValues();
}

void Analyze::statsYZoom(double zoomAmount)
{
    auto axis = activeYAxis;
    if (!axis) return;
    auto range = axis->range();
    const double halfDiff = (range.upper - range.lower) / 2.0;
    const double middle = (range.upper + range.lower) / 2.0;
    axis->setRange(QCPRange(middle - halfDiff * zoomAmount, middle + halfDiff * zoomAmount));
    if (m_YAxisTool.isVisible() && m_YAxisTool.getAxis() == axis)
        m_YAxisTool.replot(true);
}
void Analyze::statsYZoomIn()
{
    statsYZoom(0.80);
    statsPlot->replot();
}
void Analyze::statsYZoomOut()
{
    statsYZoom(1.25);
    statsPlot->replot();
}

namespace
{
// Pass in a function that converts the double graph value to a string
// for the value box.
template<typename Func>
void updateStat(double time, QLineEdit *valueBox, QCPGraph *graph, Func func, bool useLastRealVal = false)
{
    auto begin = graph->data()->findBegin(time);
    double timeDiffThreshold = 10000000.0;
    if ((begin != graph->data()->constEnd()) &&
            (fabs(begin->mainKey() - time) < timeDiffThreshold))
    {
        double foundVal = begin->mainValue();
        valueBox->setDisabled(false);
        if (qIsNaN(foundVal))
        {
            int index = graph->findBegin(time);
            const double MAX_TIME_DIFF = 600;
            while (useLastRealVal && index >= 0)
            {
                const double val = graph->data()->at(index)->mainValue();
                const double t = graph->data()->at(index)->mainKey();
                if (time - t > MAX_TIME_DIFF)
                    break;
                if (!qIsNaN(val))
                {
                    valueBox->setText(func(val));
                    return;
                }
                index--;
            }
            valueBox->clear();
        }
        else
            valueBox->setText(func(foundVal));
    }
    else valueBox->setDisabled(true);
}

}  // namespace

// This populates the output boxes below the stats plot with the correct statistics.
void Analyze::updateStatsValues()
{
    const double time = statsCursorTime < 0 ? maxXValue : statsCursorTime;

    auto d2Fcn = [](double d) -> QString { return QString::number(d, 'f', 2); };
    auto d1Fcn = [](double d) -> QString { return QString::number(d, 'f', 1); };
    // HFR, numCaptureStars, median & eccentricity are the only ones to use the last real value,
    // that is, it keeps those values from the last exposure.
    updateStat(time, hfrOut, statsPlot->graph(HFR_GRAPH), d2Fcn, true);
    updateStat(time, eccentricityOut, statsPlot->graph(ECCENTRICITY_GRAPH), d2Fcn, true);
    updateStat(time, skyBgOut, statsPlot->graph(SKYBG_GRAPH), d1Fcn);
    updateStat(time, snrOut, statsPlot->graph(SNR_GRAPH), d1Fcn);
    updateStat(time, raOut, statsPlot->graph(RA_GRAPH), d2Fcn);
    updateStat(time, decOut, statsPlot->graph(DEC_GRAPH), d2Fcn);
    updateStat(time, driftOut, statsPlot->graph(DRIFT_GRAPH), d2Fcn);
    updateStat(time, rmsOut, statsPlot->graph(RMS_GRAPH), d2Fcn);
    updateStat(time, rmsCOut, statsPlot->graph(CAPTURE_RMS_GRAPH), d2Fcn);
    updateStat(time, azOut, statsPlot->graph(AZ_GRAPH), d1Fcn);
    updateStat(time, altOut, statsPlot->graph(ALT_GRAPH), d2Fcn);
    updateStat(time, temperatureOut, statsPlot->graph(TEMPERATURE_GRAPH), d2Fcn);

    auto asFcn = [](double d) -> QString { return QString("%1\"").arg(d, 0, 'f', 0); };
    updateStat(time, targetDistanceOut, statsPlot->graph(TARGET_DISTANCE_GRAPH), asFcn, true);

    auto hmsFcn = [](double d) -> QString
    {
        dms ra;
        ra.setD(d);
        return QString("%1:%2:%3").arg(ra.hour()).arg(ra.minute()).arg(ra.second());
        //return ra.toHMSString();
    };
    updateStat(time, mountRaOut, statsPlot->graph(MOUNT_RA_GRAPH), hmsFcn);
    auto dmsFcn = [](double d) -> QString { dms dec; dec.setD(d); return dec.toDMSString(); };
    updateStat(time, mountDecOut, statsPlot->graph(MOUNT_DEC_GRAPH), dmsFcn);
    auto haFcn = [](double d) -> QString
    {
        dms ha;
        QChar z('0');
        QChar sgn('+');
        ha.setD(d);
        if (ha.Hours() > 12.0)
        {
            ha.setH(24.0 - ha.Hours());
            sgn = '-';
        }
        return QString("%1%2:%3").arg(sgn).arg(ha.hour(), 2, 10, z)
        .arg(ha.minute(), 2, 10, z);
    };
    updateStat(time, mountHaOut, statsPlot->graph(MOUNT_HA_GRAPH), haFcn);

    auto intFcn = [](double d) -> QString { return QString::number(d, 'f', 0); };
    updateStat(time, numStarsOut, statsPlot->graph(NUMSTARS_GRAPH), intFcn);
    updateStat(time, raPulseOut, statsPlot->graph(RA_PULSE_GRAPH), intFcn);
    updateStat(time, decPulseOut, statsPlot->graph(DEC_PULSE_GRAPH), intFcn);
    updateStat(time, numCaptureStarsOut, statsPlot->graph(NUM_CAPTURE_STARS_GRAPH), intFcn, true);
    updateStat(time, medianOut, statsPlot->graph(MEDIAN_GRAPH), intFcn, true);
    updateStat(time, focusPositionOut, statsPlot->graph(FOCUS_POSITION_GRAPH), intFcn);

    auto pierFcn = [](double d) -> QString
    {
        return d == 0.0 ? "W->E" : d == 1.0 ? "E->W" : "?";
    };
    updateStat(time, pierSideOut, statsPlot->graph(PIER_SIDE_GRAPH), pierFcn);
}

void Analyze::initStatsCheckboxes()
{
    hfrCB->setChecked(Options::analyzeHFR());
    numCaptureStarsCB->setChecked(Options::analyzeNumCaptureStars());
    medianCB->setChecked(Options::analyzeMedian());
    eccentricityCB->setChecked(Options::analyzeEccentricity());
    numStarsCB->setChecked(Options::analyzeNumStars());
    skyBgCB->setChecked(Options::analyzeSkyBg());
    snrCB->setChecked(Options::analyzeSNR());
    temperatureCB->setChecked(Options::analyzeTemperature());
    focusPositionCB->setChecked(Options::focusPosition());
    targetDistanceCB->setChecked(Options::analyzeTargetDistance());
    raCB->setChecked(Options::analyzeRA());
    decCB->setChecked(Options::analyzeDEC());
    raPulseCB->setChecked(Options::analyzeRAp());
    decPulseCB->setChecked(Options::analyzeDECp());
    driftCB->setChecked(Options::analyzeDrift());
    rmsCB->setChecked(Options::analyzeRMS());
    rmsCCB->setChecked(Options::analyzeRMSC());
    mountRaCB->setChecked(Options::analyzeMountRA());
    mountDecCB->setChecked(Options::analyzeMountDEC());
    mountHaCB->setChecked(Options::analyzeMountHA());
    azCB->setChecked(Options::analyzeAz());
    altCB->setChecked(Options::analyzeAlt());
    pierSideCB->setChecked(Options::analyzePierSide());
}

void Analyze::zoomIn()
{
    if (plotWidth > 0.5)
    {
        if (keepCurrentCB->isChecked())
            // If we're keeping to the end of the data, keep the end on the right.
            plotStart = std::max(0.0, maxXValue - plotWidth / 4.0);
        else if (statsCursorTime >= 0)
            // If there is a cursor, try to move it to the center.
            plotStart = std::max(0.0, statsCursorTime - plotWidth / 4.0);
        else
            // Keep the center the same.
            plotStart += plotWidth / 4.0;
        plotWidth = plotWidth / 2.0;
    }
    fullWidthCB->setChecked(false);
    replot();
}

void Analyze::zoomOut()
{
    if (plotWidth < maxXValue)
    {
        plotStart = std::max(0.0, plotStart - plotWidth / 2.0);
        plotWidth = plotWidth * 2;
    }
    fullWidthCB->setChecked(false);
    replot();
}

namespace
{

void setupAxisDefaults(QCPAxis *axis)
{
    axis->setBasePen(QPen(Qt::white, 1));
    axis->grid()->setPen(QPen(QColor(140, 140, 140, 140), 1, Qt::DotLine));
    axis->grid()->setSubGridPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
    axis->grid()->setZeroLinePen(Qt::NoPen);
    axis->setBasePen(QPen(Qt::white, 1));
    axis->setTickPen(QPen(Qt::white, 1));
    axis->setSubTickPen(QPen(Qt::white, 1));
    axis->setTickLabelColor(Qt::white);
    axis->setLabelColor(Qt::white);
    axis->grid()->setVisible(true);
}

// Generic initialization of a plot, applied to all plots in this tab.
void initQCP(QCustomPlot *plot)
{
    plot->setBackground(QBrush(Qt::black));
    setupAxisDefaults(plot->yAxis);
    setupAxisDefaults(plot->xAxis);
    plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
}
}  // namespace

void Analyze::initTimelinePlot()
{
    initQCP(timelinePlot);

    // This places the labels on the left of the timeline.
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    textTicker->addTick(CAPTURE_Y, i18n("Capture"));
    textTicker->addTick(FOCUS_Y, i18n("Focus"));
    textTicker->addTick(ALIGN_Y, i18n("Align"));
    textTicker->addTick(GUIDE_Y, i18n("Guide"));
    textTicker->addTick(MERIDIAN_MOUNT_FLIP_Y, i18n("Flip"));
    textTicker->addTick(MOUNT_Y, i18n("Mount"));
    textTicker->addTick(SCHEDULER_Y, i18n("Job"));
    timelinePlot->yAxis->setTicker(textTicker);

    ADAPTIVE_FOCUS_GRAPH = initGraph(timelinePlot, timelinePlot->yAxis, QCPGraph::lsNone, Qt::red, "adaptiveFocus");
    timelinePlot->graph(ADAPTIVE_FOCUS_GRAPH)->setPen(QPen(Qt::red, 2));
    timelinePlot->graph(ADAPTIVE_FOCUS_GRAPH)->setScatterStyle(QCPScatterStyle::ssDisc);
}

// Turn on and off the various statistics, adding/removing them from the legend.
void Analyze::toggleGraph(int graph_id, bool show)
{
    statsPlot->graph(graph_id)->setVisible(show);
    if (show)
        statsPlot->graph(graph_id)->addToLegend();
    else
        statsPlot->graph(graph_id)->removeFromLegend();
    replot();
}

int Analyze::initGraph(QCustomPlot * plot, QCPAxis * yAxis, QCPGraph::LineStyle lineStyle,
                       const QColor &color, const QString &name)
{
    int num = plot->graphCount();
    plot->addGraph(plot->xAxis, yAxis);
    plot->graph(num)->setLineStyle(lineStyle);
    plot->graph(num)->setPen(QPen(color));
    plot->graph(num)->setName(name);
    return num;
}

void Analyze::updateYAxisMap(QObject * key, const YAxisInfo &axisInfo)
{
    if (key == nullptr) return;
    auto axisEntry = yAxisMap.find(key);
    if (axisEntry == yAxisMap.end())
        yAxisMap.insert(std::make_pair(key, axisInfo));
    else
        axisEntry->second = axisInfo;
}

template <typename Func>
int Analyze::initGraphAndCB(QCustomPlot * plot, QCPAxis * yAxis, QCPGraph::LineStyle lineStyle,
                            const QColor &color, const QString &name, const QString &shortName,
                            QCheckBox * cb, Func setCb, QLineEdit * out)
{
    const int num = initGraph(plot, yAxis, lineStyle, color, shortName);
    if (out != nullptr)
    {
        const bool autoAxis = YAxisInfo::isRescale(yAxis->range());
        updateYAxisMap(out, YAxisInfo(yAxis, yAxis->range(), autoAxis, num, plot, cb, name, shortName, color));
    }
    if (cb != nullptr)
    {
        // Don't call toggleGraph() here, as it's too early for replot().
        bool show = cb->isChecked();
        plot->graph(num)->setVisible(show);
        if (show)
            plot->graph(num)->addToLegend();
        else
            plot->graph(num)->removeFromLegend();

        connect(cb, &QCheckBox::toggled,
                [ = ](bool show)
        {
            this->toggleGraph(num, show);
            setCb(show);
        });
    }
    return num;
}


void Analyze::userSetAxisColor(QObject *key, const YAxisInfo &axisInfo, const QColor &color)
{
    updateYAxisMap(key, axisInfo);
    statsPlot->graph(axisInfo.graphIndex)->setPen(QPen(color));
    Options::setAnalyzeStatsYAxis(serializeYAxes());
    replot();
}

void Analyze::userSetLeftAxis(QCPAxis *axis)
{
    setLeftAxis(axis);
    Options::setAnalyzeStatsYAxis(serializeYAxes());
    replot();
}

void Analyze::userChangedYAxis(QObject *key, const YAxisInfo &axisInfo)
{
    updateYAxisMap(key, axisInfo);
    Options::setAnalyzeStatsYAxis(serializeYAxes());
    replot();
}

// TODO: Doesn't seem like this is ever getting called. Not sure why not receiving the rangeChanged signal.
void Analyze::yAxisRangeChanged(const QCPRange &newRange)
{
    Q_UNUSED(newRange);
    if (m_YAxisTool.isVisible() && m_YAxisTool.getAxis() == activeYAxis)
        m_YAxisTool.replot(true);
}

void Analyze::setLeftAxis(QCPAxis *axis)
{
    if (axis != nullptr && axis != activeYAxis)
    {
        for (const auto &pair : yAxisMap)
        {
            disconnect(pair.second.axis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this,
                       QOverload<const QCPRange &>::of(&Analyze::yAxisRangeChanged));
            pair.second.axis->setVisible(false);
        }
        axis->setVisible(true);
        activeYAxis = axis;
        statsPlot->axisRect()->setRangeZoomAxes(0, axis);
        connect(axis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this,
                QOverload<const QCPRange &>::of(&Analyze::yAxisRangeChanged));
    }
}

void Analyze::startYAxisTool(QObject * key, const YAxisInfo &info)
{
    if (info.checkBox && !info.checkBox->isChecked())
    {
        // Enable the graph.
        info.checkBox->setChecked(true);
        statsPlot->graph(info.graphIndex)->setVisible(true);
        statsPlot->graph(info.graphIndex)->addToLegend();
    }

    m_YAxisTool.reset(key, info, info.axis == activeYAxis);
    m_YAxisTool.show();
}

QCPAxis *Analyze::newStatsYAxis(const QString &label, double lower, double upper)
{
    QCPAxis *axis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0); // 0 means QCP creates the axis.
    axis->setVisible(false);
    axis->setRange(lower, upper);
    axis->setLabel(label);
    setupAxisDefaults(axis);
    return axis;
}

bool Analyze::restoreYAxes(const QString &encoding)
{
    constexpr int headerSize = 2;
    constexpr int itemSize = 5;
    QStringList items = encoding.split(',');
    if (items.size() <= headerSize) return false;
    if ((items.size() - headerSize) % itemSize != 0) return false;
    if (items[0] != "AnalyzeStatsYAxis1.0") return false;

    // Restore the active Y axis
    const QString leftID = "left=";
    if (!items[1].startsWith(leftID)) return false;
    QString left = items[1].mid(leftID.size());
    if (left.size() <= 0) return false;
    for (const auto &pair : yAxisMap)
    {
        if (pair.second.axis->label() == left)
        {
            setLeftAxis(pair.second.axis);
            break;
        }
    }

    // Restore the various upper/lower/rescale axis values.
    for (int i = headerSize; i < items.size(); i += itemSize)
    {
        const QString shortName = items[i];
        const double lower = items[i + 1].toDouble();
        const double upper = items[i + 2].toDouble();
        const bool rescale = items[i + 3] == "T";
        const QColor color(items[i + 4]);
        for (auto &pair : yAxisMap)
        {
            auto &info = pair.second;
            if (info.axis->label() == shortName)
            {
                info.color = color;
                statsPlot->graph(info.graphIndex)->setPen(QPen(color));
                info.rescale = rescale;
                if (rescale)
                    info.axis->setRange(
                        QCPRange(YAxisInfo::LOWER_RESCALE,
                                 YAxisInfo::UPPER_RESCALE));
                else
                    info.axis->setRange(QCPRange(lower, upper));
                break;
            }
        }
    }
    return true;
}

// This would be sensitive to short names with commas in them, but we don't do that.
QString Analyze::serializeYAxes()
{
    QString encoding = QString("AnalyzeStatsYAxis1.0,left=%1").arg(activeYAxis->label());
    QList<QString> savedAxes;
    for (const auto &pair : yAxisMap)
    {
        const YAxisInfo &info = pair.second;
        const bool rescale = info.rescale;

        // Only save if something has changed.
        bool somethingChanged = (info.initialColor != info.color) ||
                                (rescale != YAxisInfo::isRescale(info.initialRange)) ||
                                (!rescale && info.axis->range() != info.initialRange);

        if (!somethingChanged) continue;

        // Don't save the same axis twice
        if (savedAxes.contains(info.axis->label())) continue;

        double lower = rescale ? YAxisInfo::LOWER_RESCALE : info.axis->range().lower;
        double upper = rescale ? YAxisInfo::UPPER_RESCALE : info.axis->range().upper;
        encoding.append(QString(",%1,%2,%3,%4,%5")
                        .arg(info.axis->label()).arg(lower).arg(upper)
                        .arg(info.rescale ? "T" : "F").arg(info.color.name()));
        savedAxes.append(info.axis->label());
    }
    return encoding;
}

void Analyze::initStatsPlot()
{
    initQCP(statsPlot);

    // Setup the main y-axis
    statsPlot->yAxis->setVisible(true);
    statsPlot->yAxis->setLabel("RA/DEC");
    statsPlot->yAxis->setRange(-2, 5);
    setLeftAxis(statsPlot->yAxis);

    // Setup the legend
    statsPlot->legend->setVisible(true);
    statsPlot->legend->setFont(QFont("Helvetica", 6));
    statsPlot->legend->setTextColor(Qt::white);
    // Legend background is transparent.
    statsPlot->legend->setBrush(QBrush(QColor(0, 0, 0, 50)));
    // Legend stacks vertically.
    statsPlot->legend->setFillOrder(QCPLegend::foRowsFirst);
    // Rows pretty tightly packed.
    statsPlot->legend->setRowSpacing(-10);

    statsPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft | Qt::AlignTop);
    statsPlot->legend->setSelectableParts(QCPLegend::spLegendBox);

    // Make the lines part of the legend less long.
    statsPlot->legend->setIconSize(10, 18);
    statsPlot->legend->setIconTextPadding(3);

    // Clicking on the legend makes it very small (and thus less obscuring the plot).
    // Clicking again restores it to its original size.
    connect(statsPlot, &QCustomPlot::legendClick, [ = ](QCPLegend * legend, QCPAbstractLegendItem * item, QMouseEvent * event)
    {
        Q_UNUSED(legend);
        Q_UNUSED(item);
        Q_UNUSED(event);
        if (statsPlot->legend->font().pointSize() < 6)
        {
            // Restore the original legend.
            statsPlot->legend->setRowSpacing(-10);
            statsPlot->legend->setIconSize(10, 18);
            statsPlot->legend->setFont(QFont("Helvetica", 6));
            statsPlot->legend->setBrush(QBrush(QColor(0, 0, 0, 50)));
        }
        else
        {
            // Make the legend very small (unreadable, but clickable).
            statsPlot->legend->setRowSpacing(-10);
            statsPlot->legend->setIconSize(5, 5);
            statsPlot->legend->setFont(QFont("Helvetica", 1));
            statsPlot->legend->setBrush(QBrush(QColor(0, 0, 0, 0)));
        }
        statsPlot->replot();
    });


    // Add the graphs.
    QString shortName = "HFR";
    QCPAxis *hfrAxis = newStatsYAxis(shortName, -2, 6);
    HFR_GRAPH = initGraphAndCB(statsPlot, hfrAxis, QCPGraph::lsStepRight, Qt::cyan, "Capture Image HFR", shortName, hfrCB,
                               Options::setAnalyzeHFR, hfrOut);
    connect(hfrCB, &QCheckBox::clicked,
            [ = ](bool show)
    {
        if (show && !Options::autoHFR())
            KSNotification::info(
                i18n("The \"Auto Compute HFR\" option in the KStars "
                     "FITS options menu is not set. You won't get HFR values "
                     "without it. Once you set it, newly captured images "
                     "will have their HFRs computed."));
    });

    shortName = "#SubStars";
    QCPAxis *numCaptureStarsAxis = newStatsYAxis(shortName);
    NUM_CAPTURE_STARS_GRAPH = initGraphAndCB(statsPlot, numCaptureStarsAxis, QCPGraph::lsStepRight, Qt::darkGreen,
                              "#Stars in Capture", shortName,
                              numCaptureStarsCB, Options::setAnalyzeNumCaptureStars, numCaptureStarsOut);
    connect(numCaptureStarsCB, &QCheckBox::clicked,
            [ = ](bool show)
    {
        if (show && !Options::autoHFR())
            KSNotification::info(
                i18n("The \"Auto Compute HFR\" option in the KStars "
                     "FITS options menu is not set. You won't get # stars in capture image values "
                     "without it. Once you set it, newly captured images "
                     "will have their stars detected."));
    });

    shortName = "median";
    QCPAxis *medianAxis = newStatsYAxis(shortName);
    MEDIAN_GRAPH = initGraphAndCB(statsPlot, medianAxis, QCPGraph::lsStepRight, Qt::darkGray, "Median Pixel", shortName,
                                  medianCB, Options::setAnalyzeMedian, medianOut);

    shortName = "ecc";
    QCPAxis *eccAxis = newStatsYAxis(shortName, 0, 1.0);
    ECCENTRICITY_GRAPH = initGraphAndCB(statsPlot, eccAxis, QCPGraph::lsStepRight, Qt::darkMagenta, "Eccentricity",
                                        shortName, eccentricityCB, Options::setAnalyzeEccentricity, eccentricityOut);
    shortName = "#Stars";
    QCPAxis *numStarsAxis = newStatsYAxis(shortName);
    NUMSTARS_GRAPH = initGraphAndCB(statsPlot, numStarsAxis, QCPGraph::lsStepRight, Qt::magenta, "#Stars in Guide Image",
                                    shortName, numStarsCB, Options::setAnalyzeNumStars, numStarsOut);
    shortName = "SkyBG";
    QCPAxis *skyBgAxis = newStatsYAxis(shortName);
    SKYBG_GRAPH = initGraphAndCB(statsPlot, skyBgAxis, QCPGraph::lsStepRight, Qt::darkYellow, "Sky Background Brightness",
                                 shortName, skyBgCB, Options::setAnalyzeSkyBg, skyBgOut);

    shortName = "temp";
    QCPAxis *temperatureAxis = newStatsYAxis(shortName, -40, 40);
    TEMPERATURE_GRAPH = initGraphAndCB(statsPlot, temperatureAxis, QCPGraph::lsLine, Qt::yellow, "Temperature", shortName,
                                       temperatureCB, Options::setAnalyzeTemperature, temperatureOut);
    shortName = "focus";
    QCPAxis *focusPositionAxis = newStatsYAxis(shortName);
    FOCUS_POSITION_GRAPH = initGraphAndCB(statsPlot, focusPositionAxis, QCPGraph::lsStepLeft, Qt::lightGray, "Focus", shortName,
                                          focusPositionCB, Options::setFocusPosition, focusPositionOut);
    shortName = "tDist";
    QCPAxis *targetDistanceAxis = newStatsYAxis(shortName, 0, 60);
    TARGET_DISTANCE_GRAPH = initGraphAndCB(statsPlot, targetDistanceAxis, QCPGraph::lsLine,
                                           QColor(253, 185, 200),  // pink
                                           "Distance to Target (arcsec)", shortName, targetDistanceCB, Options::setAnalyzeTargetDistance, targetDistanceOut);
    shortName = "SNR";
    QCPAxis *snrAxis = newStatsYAxis(shortName, -100, 100);
    SNR_GRAPH = initGraphAndCB(statsPlot, snrAxis, QCPGraph::lsLine, Qt::yellow, "Guider SNR", shortName, snrCB,
                               Options::setAnalyzeSNR, snrOut);
    shortName = "RA";
    auto raColor = KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError");
    RA_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, raColor, "Guider RA Drift", shortName, raCB,
                              Options::setAnalyzeRA, raOut);
    shortName = "DEC";
    auto decColor = KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError");
    DEC_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, decColor, "Guider DEC Drift", shortName, decCB,
                               Options::setAnalyzeDEC, decOut);
    shortName = "RAp";
    auto raPulseColor = KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError");
    raPulseColor.setAlpha(75);
    QCPAxis *pulseAxis = newStatsYAxis(shortName, -2 * 150, 5 * 150);
    RA_PULSE_GRAPH = initGraphAndCB(statsPlot, pulseAxis, QCPGraph::lsLine, raPulseColor, "RA Correction Pulse (ms)", shortName,
                                    raPulseCB, Options::setAnalyzeRAp, raPulseOut);
    statsPlot->graph(RA_PULSE_GRAPH)->setBrush(QBrush(raPulseColor, Qt::Dense4Pattern));

    shortName = "DECp";
    auto decPulseColor = KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError");
    decPulseColor.setAlpha(75);
    DEC_PULSE_GRAPH = initGraphAndCB(statsPlot, pulseAxis, QCPGraph::lsLine, decPulseColor, "DEC Correction Pulse (ms)",
                                     shortName, decPulseCB, Options::setAnalyzeDECp, decPulseOut);
    statsPlot->graph(DEC_PULSE_GRAPH)->setBrush(QBrush(decPulseColor, Qt::Dense4Pattern));

    shortName = "Drift";
    DRIFT_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, Qt::lightGray, "Guider Instantaneous Drift",
                                 shortName, driftCB, Options::setAnalyzeDrift, driftOut);
    shortName = "RMS";
    RMS_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, Qt::red, "Guider RMS Drift", shortName, rmsCB,
                               Options::setAnalyzeRMS, rmsOut);
    shortName = "RMSc";
    CAPTURE_RMS_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, Qt::red,
                                       "Guider RMS Drift (during capture)", shortName, rmsCCB,
                                       Options::setAnalyzeRMSC, rmsCOut);
    shortName = "MOUNT_RA";
    QCPAxis *mountRaDecAxis = newStatsYAxis(shortName, -10, 370);
    // Colors of these two unimportant--not really plotted.
    MOUNT_RA_GRAPH = initGraphAndCB(statsPlot, mountRaDecAxis, QCPGraph::lsLine, Qt::red, "Mount RA Degrees", shortName,
                                    mountRaCB, Options::setAnalyzeMountRA, mountRaOut);
    shortName = "MOUNT_DEC";
    MOUNT_DEC_GRAPH = initGraphAndCB(statsPlot, mountRaDecAxis, QCPGraph::lsLine, Qt::red, "Mount DEC Degrees", shortName,
                                     mountDecCB, Options::setAnalyzeMountDEC, mountDecOut);
    shortName = "MOUNT_HA";
    MOUNT_HA_GRAPH = initGraphAndCB(statsPlot, mountRaDecAxis, QCPGraph::lsLine, Qt::red, "Mount Hour Angle", shortName,
                                    mountHaCB, Options::setAnalyzeMountHA, mountHaOut);
    shortName = "AZ";
    QCPAxis *azAxis = newStatsYAxis(shortName, -10, 370);
    AZ_GRAPH = initGraphAndCB(statsPlot, azAxis, QCPGraph::lsLine, Qt::darkGray, "Mount Azimuth", shortName, azCB,
                              Options::setAnalyzeAz, azOut);
    shortName = "ALT";
    QCPAxis *altAxis = newStatsYAxis(shortName, 0, 90);
    ALT_GRAPH = initGraphAndCB(statsPlot, altAxis, QCPGraph::lsLine, Qt::white, "Mount Altitude", shortName, altCB,
                               Options::setAnalyzeAlt, altOut);
    shortName = "PierSide";
    QCPAxis *pierSideAxis = newStatsYAxis(shortName, -2, 2);
    PIER_SIDE_GRAPH = initGraphAndCB(statsPlot, pierSideAxis, QCPGraph::lsLine, Qt::darkRed, "Mount Pier Side", shortName,
                                     pierSideCB, Options::setAnalyzePierSide, pierSideOut);

    // This makes mouseMove only get called when a button is pressed.
    statsPlot->setMouseTracking(false);

    // Setup the clock-time labels on the x-axis of the stats plot.
    dateTicker.reset(new OffsetDateTimeTicker);
    dateTicker->setDateTimeFormat("hh:mm:ss");
    statsPlot->xAxis->setTicker(dateTicker);

    // Didn't include QCP::iRangeDrag as it  interacts poorly with the curson logic.
    statsPlot->setInteractions(QCP::iRangeZoom);

    restoreYAxes(Options::analyzeStatsYAxis());
}

// Clear the graphics and state when changing input data.
void Analyze::reset()
{
    maxXValue = 10.0;
    plotStart = 0.0;
    plotWidth = 10.0;

    guiderRms->resetFilter();
    captureRms->resetFilter();

    unhighlightTimelineItem();

    for (int i = 0; i < statsPlot->graphCount(); ++i)
        statsPlot->graph(i)->data()->clear();
    statsPlot->clearItems();

    for (int i = 0; i < timelinePlot->graphCount(); ++i)
        timelinePlot->graph(i)->data()->clear();
    timelinePlot->clearItems();

    resetGraphicsPlot();

    detailsTable->clear();
    QPalette p = detailsTable->palette();
    p.setColor(QPalette::Base, Qt::black);
    p.setColor(QPalette::Text, Qt::white);
    detailsTable->setPalette(p);

    inputValue->clear();

    captureSessions.clear();
    focusSessions.clear();
    guideSessions.clear();
    mountSessions.clear();
    alignSessions.clear();
    mountFlipSessions.clear();
    schedulerJobSessions.clear();

    numStarsOut->setText("");
    skyBgOut->setText("");
    snrOut->setText("");
    temperatureOut->setText("");
    focusPositionOut->setText("");
    targetDistanceOut->setText("");
    eccentricityOut->setText("");
    medianOut->setText("");
    numCaptureStarsOut->setText("");

    raOut->setText("");
    decOut->setText("");
    driftOut->setText("");
    rmsOut->setText("");
    rmsCOut->setText("");

    removeStatsCursor();
    removeTemporarySessions();

    resetCaptureState();
    resetAutofocusState();
    resetGuideState();
    resetGuideStats();
    resetAlignState();
    resetMountState();
    resetMountCoords();
    resetMountFlipState();
    resetSchedulerJob();

    // Note: no replot().
}

void Analyze::initGraphicsPlot()
{
    initQCP(graphicsPlot);
    FOCUS_GRAPHICS = initGraph(graphicsPlot, graphicsPlot->yAxis,
                               QCPGraph::lsNone, Qt::cyan, "Focus");
    graphicsPlot->graph(FOCUS_GRAPHICS)->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::white, 14));
    errorBars = new QCPErrorBars(graphicsPlot->xAxis, graphicsPlot->yAxis);
    errorBars->setAntialiased(false);
    errorBars->setDataPlottable(graphicsPlot->graph(FOCUS_GRAPHICS));
    errorBars->setPen(QPen(QColor(180, 180, 180)));

    FOCUS_GRAPHICS_FINAL = initGraph(graphicsPlot, graphicsPlot->yAxis,
                                     QCPGraph::lsNone, Qt::cyan, "FocusBest");
    graphicsPlot->graph(FOCUS_GRAPHICS_FINAL)->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::yellow, Qt::yellow, 14));
    finalErrorBars = new QCPErrorBars(graphicsPlot->xAxis, graphicsPlot->yAxis);
    finalErrorBars->setAntialiased(false);
    finalErrorBars->setDataPlottable(graphicsPlot->graph(FOCUS_GRAPHICS_FINAL));
    finalErrorBars->setPen(QPen(QColor(180, 180, 180)));

    FOCUS_GRAPHICS_CURVE = initGraph(graphicsPlot, graphicsPlot->yAxis,
                                     QCPGraph::lsLine, Qt::white, "FocusCurve");
    graphicsPlot->setInteractions(QCP::iRangeZoom);
    graphicsPlot->setInteraction(QCP::iRangeDrag, true);

    GUIDER_GRAPHICS = initGraph(graphicsPlot, graphicsPlot->yAxis,
                                QCPGraph::lsNone, Qt::cyan, "Guide Error");
    graphicsPlot->graph(GUIDER_GRAPHICS)->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssStar, Qt::gray, 5));
}

void Analyze::displayFocusGraphics(const QVector<double> &positions, const QVector<double> &hfrs, const bool useWeights,
                                   const QVector<double> &weights, const QVector<bool> &outliers, const QString &curve, const QString &title, bool success)
{
    resetGraphicsPlot();
    auto graph = graphicsPlot->graph(FOCUS_GRAPHICS);
    auto finalGraph = graphicsPlot->graph(FOCUS_GRAPHICS_FINAL);
    double maxHfr = -1e8, maxPosition = -1e8, minHfr = 1e8, minPosition = 1e8;
    QVector<double> errorData, finalErrorData;
    for (int i = 0; i < positions.size(); ++i)
    {
        // Yellow circle for the final point.
        if (success && i == positions.size() - 1)
        {
            finalGraph->addData(positions[i], hfrs[i]);
            if (useWeights)
            {
                // Display the error bars in Standard Deviation form = 1 / sqrt(weight)
                double sd = (weights[i] <= 0.0) ? 0.0 : std::pow(weights[i], -0.5);
                finalErrorData.push_front(sd);
            }
        }
        else
        {
            graph->addData(positions[i], hfrs[i]);
            if (useWeights)
            {
                double sd = (weights[i] <= 0.0) ? 0.0 : std::pow(weights[i], -0.5);
                errorData.push_front(sd);
            }
        }
        maxHfr = std::max(maxHfr, hfrs[i]);
        minHfr = std::min(minHfr, hfrs[i]);
        maxPosition = std::max(maxPosition, positions[i]);
        minPosition = std::min(minPosition, positions[i]);
    }

    for (int i = 0; i < positions.size(); ++i)
    {
        QCPItemText *textLabel = new QCPItemText(graphicsPlot);
        textLabel->setPositionAlignment(Qt::AlignCenter | Qt::AlignHCenter);
        textLabel->position->setType(QCPItemPosition::ptPlotCoords);
        textLabel->position->setCoords(positions[i], hfrs[i]);
        if (outliers[i])
        {
            textLabel->setText("X");
            textLabel->setColor(Qt::black);
            textLabel->setFont(QFont(font().family(), 20));
        }
        else
        {
            textLabel->setText(QString::number(i + 1));
            textLabel->setColor(Qt::red);
            textLabel->setFont(QFont(font().family(), 12));
        }
        textLabel->setPen(Qt::NoPen);
    }

    // Error bars on the focus datapoints
    errorBars->setVisible(useWeights);
    finalErrorBars->setVisible(useWeights);
    if (useWeights)
    {
        errorBars->setData(errorData);
        finalErrorBars->setData(finalErrorData);
    }

    const double xRange = maxPosition - minPosition;
    const double xPadding = hfrs.size() > 1 ? xRange / (hfrs.size() - 1.0) : 10;

    // Draw the curve, if given.
    if (curve.size() > 0)
    {
        CurveFitting curveFitting(curve);
        const double interval = xRange / 20.0;
        auto curveGraph = graphicsPlot->graph(FOCUS_GRAPHICS_CURVE);
        for (double x = minPosition - xPadding ; x <= maxPosition + xPadding; x += interval)
            curveGraph->addData(x, curveFitting.f(x));
    }

    auto plotTitle = new QCPItemText(graphicsPlot);
    plotTitle->setColor(QColor(255, 255, 255));
    plotTitle->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    plotTitle->position->setType(QCPItemPosition::ptAxisRectRatio);
    plotTitle->position->setCoords(0.5, 0);
    plotTitle->setFont(QFont(font().family(), 10));
    plotTitle->setVisible(true);
    plotTitle->setText(title);

    // Set the same axes ranges as are used in focushfrvplot.cpp.
    const double upper = 1.5 * maxHfr;
    const double lower = minHfr - (0.25 * (upper - minHfr));
    graphicsPlot->xAxis->setRange(minPosition - xPadding, maxPosition + xPadding);
    graphicsPlot->yAxis->setRange(lower, upper);
    graphicsPlot->replot();
}

void Analyze::resetGraphicsPlot()
{
    for (int i = 0; i < graphicsPlot->graphCount(); ++i)
        graphicsPlot->graph(i)->data()->clear();
    graphicsPlot->clearItems();
    errorBars->data().clear();
    finalErrorBars->data().clear();
}

void Analyze::displayFITS(const QString &filename)
{
    QUrl url = QUrl::fromLocalFile(filename);

    if (fitsViewer.isNull())
    {
        fitsViewer = KStars::Instance()->createFITSViewer();
        fitsViewerTabID = fitsViewer->loadFile(url);
        connect(fitsViewer.get(), &FITSViewer::terminated, this, [this]()
        {
            fitsViewer.clear();
        });
    }
    else
    {
        if (fitsViewer->tabExists(fitsViewerTabID))
            fitsViewer->updateFile(url, fitsViewerTabID);
        else
            fitsViewerTabID = fitsViewer->loadFile(url);
    }

    fitsViewer->show();
}

// This is intended for recording data to file.
// Don't use this when displaying data read from file, as this is not using the
// correct analyzeStartTime.
double Analyze::logTime(const QDateTime &time)
{
    if (!logInitialized)
        startLog();
    return (time.toMSecsSinceEpoch() - analyzeStartTime.toMSecsSinceEpoch()) / 1000.0;
}

// The logTime using clock = now.
// This is intended for recording data to file.
// Don't use this When displaying data read from file.
double Analyze::logTime()
{
    return logTime(QDateTime::currentDateTime());
}

// Goes back to clock time from seconds into the log.
// Appropriate for both displaying data from files as well as when displaying live data.
QDateTime Analyze::clockTime(double logSeconds)
{
    return displayStartTime.addMSecs(logSeconds * 1000.0);
}


// Write the command name, a timestamp and the message with comma separation to a .analyze file.
void Analyze::saveMessage(const QString &type, const QString &message)
{
    QString line(QString("%1,%2%3%4\n")
                 .arg(type)
                 .arg(QString::number(logTime(), 'f', 3))
                 .arg(message.size() > 0 ? "," : "", message));
    appendToLog(line);
}

// Start writing a new .analyze file and reset the graphics to start from "now".
void Analyze::restart()
{
    qCDebug(KSTARS_EKOS_ANALYZE) << "(Re)starting Analyze";

    // Setup the new .analyze file
    startLog();

    // Reset the graphics so that it ignore any old data.
    reset();
    inputCombo->setCurrentIndex(0);
    inputValue->setText("");
    maxXValue = readDataFromFile(logFilename);
    runtimeDisplay = true;
    fullWidthCB->setChecked(true);
    fullWidthCB->setVisible(true);
    fullWidthCB->setDisabled(false);
    replot();
}

// Start writing a .analyze file.
void Analyze::startLog()
{
    analyzeStartTime = QDateTime::currentDateTime();
    startTimeInitialized = true;
    if (runtimeDisplay)
        displayStartTime = analyzeStartTime;

    QDir dir = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/analyze");
    dir.mkpath(".");

    logFile.reset(new QFile);
    logFilename = dir.filePath("ekos-" + QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss") + ".analyze");
    logFile->setFileName(logFilename);
    logFile->open(QIODevice::WriteOnly | QIODevice::Text);

    // This must happen before the below appendToLog() call.
    logInitialized = true;

    appendToLog(QString("#KStars version %1. Analyze log version 1.0.\n\n")
                .arg(KSTARS_VERSION));
    appendToLog(QString("%1,%2,%3\n")
                .arg("AnalyzeStartTime", analyzeStartTime.toString(timeFormat), analyzeStartTime.timeZoneAbbreviation()));
}

void Analyze::appendToLog(const QString &lines)
{
    if (!logInitialized)
        startLog();
    QTextStream out(logFile.data());
    out << lines;
    out.flush();
}

// maxXValue is the largest time value we have seen so far for this data.
void Analyze::updateMaxX(double time)
{
    maxXValue = std::max(time, maxXValue);
}

// Manage temporary sessions displayed on the Timeline.
// Those are ongoing sessions that will ultimately be replaced when the session is complete.
// This only happens with live data, not with data read from .analyze files.

// Remove the graphic element.
void Analyze::removeTemporarySession(Session * session)
{
    if (session->rect != nullptr)
        timelinePlot->removeItem(session->rect);
    session->rect = nullptr;
    session->start = 0;
    session->end = 0;
}

// Remove all temporary sessions (i.e. from all lines in the Timeline).
void Analyze::removeTemporarySessions()
{
    removeTemporarySession(&temporaryCaptureSession);
    removeTemporarySession(&temporaryMountFlipSession);
    removeTemporarySession(&temporaryFocusSession);
    removeTemporarySession(&temporaryGuideSession);
    removeTemporarySession(&temporaryMountSession);
    removeTemporarySession(&temporaryAlignSession);
    removeTemporarySession(&temporarySchedulerJobSession);
}

// Add a new temporary session.
void Analyze::addTemporarySession(Session * session, double time, double duration,
                                  int y_offset, const QBrush &brush)
{
    if (time < 0) return;
    removeTemporarySession(session);
    session->rect = addSession(time, time + duration, y_offset, brush);
    session->start = time;
    session->end = time + duration;
    session->offset = y_offset;
    session->temporaryBrush = brush;
    updateMaxX(time + duration);
}

// Extend a temporary session. That is, we don't know how long the session will last,
// so when new data arrives (from any module, not necessarily the one with the temporary
// session) we must extend that temporary session.
void Analyze::adjustTemporarySession(Session * session)
{
    if (session->rect != nullptr && session->end < maxXValue)
    {
        QBrush brush = session->temporaryBrush;
        double start = session->start;
        int offset = session->offset;
        addTemporarySession(session, start, maxXValue - start, offset, brush);
    }
}

// Extend all temporary sessions.
void Analyze::adjustTemporarySessions()
{
    adjustTemporarySession(&temporaryCaptureSession);
    adjustTemporarySession(&temporaryMountFlipSession);
    adjustTemporarySession(&temporaryFocusSession);
    adjustTemporarySession(&temporaryGuideSession);
    adjustTemporarySession(&temporaryMountSession);
    adjustTemporarySession(&temporaryAlignSession);
    adjustTemporarySession(&temporarySchedulerJobSession);
}

// Called when the captureStarting slot receives a signal.
// Saves the message to disk, and calls processCaptureStarting.
void Analyze::captureStarting(double exposureSeconds, const QString &filter)
{
    saveMessage("CaptureStarting",
                QString("%1,%2").arg(QString::number(exposureSeconds, 'f', 3), filter));
    processCaptureStarting(logTime(), exposureSeconds, filter);
}

// Called by either the above (when live data is received), or reading from file.
// BatchMode would be true when reading from file.
void Analyze::processCaptureStarting(double time, double exposureSeconds, const QString &filter)
{
    captureStartedTime = time;
    captureStartedFilter = filter;
    updateMaxX(time);

    addTemporarySession(&temporaryCaptureSession, time, 1, CAPTURE_Y, temporaryBrush);
    temporaryCaptureSession.duration = exposureSeconds;
    temporaryCaptureSession.filter = filter;
}

// Called when the captureComplete slot receives a signal.
void Analyze::captureComplete(const QVariantMap &metadata)
{
    auto filename = metadata["filename"].toString();
    auto exposure = metadata["exposure"].toDouble();
    auto filter = metadata["filter"].toString();
    auto hfr = metadata["hfr"].toDouble();
    auto starCount = metadata["starCount"].toInt();
    auto median = metadata["median"].toDouble();
    auto eccentricity = metadata["eccentricity"].toDouble();

    saveMessage("CaptureComplete",
                QString("%1,%2,%3,%4,%5,%6,%7")
                .arg(QString::number(exposure, 'f', 3), filter, QString::number(hfr, 'f', 3), filename)
                .arg(starCount)
                .arg(median)
                .arg(QString::number(eccentricity, 'f', 3)));
    if (runtimeDisplay && captureStartedTime >= 0)
        processCaptureComplete(logTime(), filename, exposure, filter, hfr, starCount, median, eccentricity);
}

void Analyze::processCaptureComplete(double time, const QString &filename,
                                     double exposureSeconds, const QString &filter, double hfr,
                                     int numStars, int median, double eccentricity, bool batchMode)
{
    removeTemporarySession(&temporaryCaptureSession);
    QBrush stripe;
    if (captureStartedTime < 0)
        return;

    if (filterStripeBrush(filter, &stripe))
        addSession(captureStartedTime, time, CAPTURE_Y, successBrush, &stripe);
    else
        addSession(captureStartedTime, time, CAPTURE_Y, successBrush, nullptr);
    auto session = CaptureSession(captureStartedTime, time, nullptr, false,
                                  filename, exposureSeconds, filter);
    captureSessions.add(session);
    addHFR(hfr, numStars, median, eccentricity, time, captureStartedTime);
    updateMaxX(time);
    if (!batchMode)
    {
        if (runtimeDisplay && keepCurrentCB->isChecked() && statsCursor == nullptr)
            captureSessionClicked(session, false);
        replot();
    }
    previousCaptureStartedTime = captureStartedTime;
    previousCaptureCompletedTime = time;
    captureStartedTime = -1;
}

void Analyze::captureAborted(double exposureSeconds)
{
    saveMessage("CaptureAborted",
                QString("%1").arg(QString::number(exposureSeconds, 'f', 3)));
    if (runtimeDisplay && captureStartedTime >= 0)
        processCaptureAborted(logTime(), exposureSeconds);
}

void Analyze::processCaptureAborted(double time, double exposureSeconds, bool batchMode)
{
    removeTemporarySession(&temporaryCaptureSession);
    double duration = time - captureStartedTime;
    if (captureStartedTime >= 0 &&
            duration < (exposureSeconds + 30) &&
            duration < 3600)
    {
        // You can get a captureAborted without a captureStarting,
        // so make sure this associates with a real start.
        addSession(captureStartedTime, time, CAPTURE_Y, failureBrush);
        auto session = CaptureSession(captureStartedTime, time, nullptr, true, "",
                                      exposureSeconds, captureStartedFilter);
        captureSessions.add(session);
        updateMaxX(time);
        if (!batchMode)
        {
            if (runtimeDisplay && keepCurrentCB->isChecked() && statsCursor == nullptr)
                captureSessionClicked(session, false);
            replot();
        }
        captureStartedTime = -1;
    }
    previousCaptureStartedTime = -1;
    previousCaptureCompletedTime = -1;
}

void Analyze::resetCaptureState()
{
    captureStartedTime = -1;
    captureStartedFilter = "";
    medianMax = 1;
    numCaptureStarsMax = 1;
    previousCaptureStartedTime = -1;
    previousCaptureCompletedTime = -1;
}

void Analyze::autofocusStarting(double temperature, const QString &filter, const AutofocusReason reason,
                                const QString &reasonInfo)
{
    saveMessage("AutofocusStarting",
                QString("%1,%2,%3,%4")
                .arg(filter)
                .arg(QString::number(temperature, 'f', 1))
                .arg(QString::number(reason))
                .arg(reasonInfo));
    processAutofocusStarting(logTime(), temperature, filter, reason, reasonInfo);
}

void Analyze::processAutofocusStarting(double time, double temperature, const QString &filter, const AutofocusReason reason,
                                       const QString &reasonInfo)
{
    autofocusStartedTime = time;
    autofocusStartedFilter = filter;
    autofocusStartedTemperature = temperature;
    autofocusStartedReason = reason;
    autofocusStartedReasonInfo = reasonInfo;

    addTemperature(temperature, time);
    updateMaxX(time);

    addTemporarySession(&temporaryFocusSession, time, 1, FOCUS_Y, temporaryBrush);
    temporaryFocusSession.temperature = temperature;
    temporaryFocusSession.filter = filter;
    temporaryFocusSession.reason = reason;
}

void Analyze::adaptiveFocusComplete(const QString &filter, double temperature, double tempTicks,
                                    double altitude, double altTicks, int prevPosError, int thisPosError,
                                    int totalTicks, int position, bool focuserMoved)
{
    saveMessage("AdaptiveFocusComplete", QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10").arg(filter).arg(temperature, 0, 'f', 2)
                .arg(tempTicks, 0, 'f', 2).arg(altitude, 0, 'f', 2).arg(altTicks, 0, 'f', 2).arg(prevPosError)
                .arg(thisPosError).arg(totalTicks).arg(position).arg(focuserMoved ? 1 : 0));

    if (runtimeDisplay)
        processAdaptiveFocusComplete(logTime(), filter, temperature, tempTicks, altitude, altTicks, prevPosError, thisPosError,
                                     totalTicks, position, focuserMoved);
}

void Analyze::processAdaptiveFocusComplete(double time, const QString &filter, double temperature, double tempTicks,
        double altitude, double altTicks, int prevPosError, int thisPosError, int totalTicks, int position,
        bool focuserMoved, bool batchMode)
{
    removeTemporarySession(&temporaryFocusSession);

    addFocusPosition(position, time);
    updateMaxX(time);

    // In general if nothing happened we won't plot a value. This means there won't be lots of points with zeros in them.
    // However, we need to cover the situation of offsetting movements that overall don't move the focuser but still have non-zero detail
    if (!focuserMoved || (abs(tempTicks) < 1.00 && abs(altTicks) < 1.0 && prevPosError == 0 && thisPosError == 0))
        return;

    // Add a dot on the timeline.
    timelinePlot->graph(ADAPTIVE_FOCUS_GRAPH)->addData(time, FOCUS_Y);

    // Add mouse sensitivity on the timeline.
    constexpr int artificialInterval = 10;
    auto session = FocusSession(time - artificialInterval, time + artificialInterval, nullptr,
                                filter, temperature, tempTicks, altitude, altTicks, prevPosError, thisPosError, totalTicks,
                                position);
    focusSessions.add(session);

    if (!batchMode)
        replot();

    autofocusStartedTime = -1;
}

void Analyze::autofocusComplete(const double temperature, const QString &filter, const QString &points,
                                const bool useWeights, const QString &curve, const QString &rawTitle)
{
    // Remove commas from the title as they're used as separators in the .analyze file.
    QString title = rawTitle;
    title.replace(",", " ");

    // Version 1 message structure is now deprecated, leaving code commented out in case old files need debugging
    /*if (curve.size() == 0)
        saveMessage("AutofocusComplete", QString("%1,%2").arg(filter, points));
    else if (title.size() == 0)
        saveMessage("AutofocusComplete", QString("%1,%2,%3").arg(filter, points, curve));
    else
        saveMessage("AutofocusComplete", QString("%1,%2,%3,%4").arg(filter, points, curve, title));*/

    QString temp = QString::number(temperature, 'f', 1);
    QVariant reasonV = autofocusStartedReason;
    QString reason = reasonV.toString();
    QString reasonInfo = autofocusStartedReasonInfo;
    QString weights = QString::number(useWeights);
    if (curve.size() == 0)
        saveMessage("AutofocusComplete", QString("%1,%2,%3,%4,%5,%6").arg(temp, reason, reasonInfo, filter, points, weights));
    else if (title.size() == 0)
        saveMessage("AutofocusComplete", QString("%1,%2,%3,%4,%5,%6,%7").arg(temp, reason, reasonInfo, filter, points, weights,
                    curve));
    else
        saveMessage("AutofocusComplete", QString("%1,%2,%3,%4,%5,%6,%7,%8").arg(temp, reason, reasonInfo, filter, points, weights,
                    curve, title));

    if (runtimeDisplay && autofocusStartedTime >= 0)
        processAutofocusCompleteV2(logTime(), temperature, filter, autofocusStartedReason, reasonInfo, points, useWeights, curve,
                                   title);
}

// Version 2 of processAutofocusComplete to process weights, outliers and reason codes.
void Analyze::processAutofocusCompleteV2(double time, const double temperature, const QString &filter,
        const AutofocusReason reason, const QString &reasonInfo,
        const QString &points, const bool useWeights, const QString &curve, const QString &title, bool batchMode)
{
    removeTemporarySession(&temporaryFocusSession);
    updateMaxX(time);
    if (autofocusStartedTime >= 0)
    {
        QBrush stripe;
        if (filterStripeBrush(filter, &stripe))
            addSession(autofocusStartedTime, time, FOCUS_Y, successBrush, &stripe);
        else
            addSession(autofocusStartedTime, time, FOCUS_Y, successBrush, nullptr);
        // Use the focus complete temperature (rather than focus start temperature) for consistency with Focus
        auto session = FocusSession(autofocusStartedTime, time, nullptr, true, temperature, filter, reason, reasonInfo, points,
                                    useWeights, curve, title, AutofocusFailReason::FOCUS_FAIL_NONE, "");
        focusSessions.add(session);
        addFocusPosition(session.focusPosition(), autofocusStartedTime);
        if (!batchMode)
        {
            if (runtimeDisplay && keepCurrentCB->isChecked() && statsCursor == nullptr)
                focusSessionClicked(session, false);
            replot();
        }
    }
    autofocusStartedTime = -1;
}

// Older version of processAutofocusComplete to process analyze files created before version 2.
void Analyze::processAutofocusComplete(double time, const QString &filter, const QString &points,
                                       const QString &curve, const QString &title, bool batchMode)
{
    removeTemporarySession(&temporaryFocusSession);
    if (autofocusStartedTime < 0)
        return;

    QBrush stripe;
    if (filterStripeBrush(filter, &stripe))
        addSession(autofocusStartedTime, time, FOCUS_Y, successBrush, &stripe);
    else
        addSession(autofocusStartedTime, time, FOCUS_Y, successBrush, nullptr);
    auto session = FocusSession(autofocusStartedTime, time, nullptr, true,
                                autofocusStartedTemperature, filter, points, curve, title);
    focusSessions.add(session);
    addFocusPosition(session.focusPosition(), autofocusStartedTime);
    updateMaxX(time);
    if (!batchMode)
    {
        if (runtimeDisplay && keepCurrentCB->isChecked() && statsCursor == nullptr)
            focusSessionClicked(session, false);
        replot();
    }
    autofocusStartedTime = -1;
}

void Analyze::autofocusAborted(const QString &filter, const QString &points, const bool useWeights,
                               const AutofocusFailReason failCode, const QString failCodeInfo)
{
    QString temperature = QString::number(autofocusStartedTemperature, 'f', 1);
    QVariant reasonV = autofocusStartedReason;
    QString reason = reasonV.toString();
    QString reasonInfo = autofocusStartedReasonInfo;
    QString weights = QString::number(useWeights);
    QVariant failReasonV = static_cast<int>(failCode);
    QString failReason = failReasonV.toString();
    saveMessage("AutofocusAborted", QString("%1,%2,%3,%4,%5,%6,%7,%8").arg(temperature, reason, reasonInfo, filter, points,
                weights, failReason, failCodeInfo));
    if (runtimeDisplay && autofocusStartedTime >= 0)
        processAutofocusAbortedV2(logTime(), autofocusStartedTemperature, filter, autofocusStartedReason, reasonInfo, points,
                                  useWeights, failCode, failCodeInfo);
}

// Version 2 of processAutofocusAborted added weights, outliers and reason codes.
void Analyze::processAutofocusAbortedV2(double time, double temperature, const QString &filter,
                                        const AutofocusReason reason, const QString &reasonInfo, const QString &points, const bool useWeights,
                                        const AutofocusFailReason failCode, const QString failCodeInfo, bool batchMode)
{
    Q_UNUSED(temperature);
    removeTemporarySession(&temporaryFocusSession);
    double duration = time - autofocusStartedTime;
    if (autofocusStartedTime >= 0 && duration < 1000)
    {
        // Just in case..
        addSession(autofocusStartedTime, time, FOCUS_Y, failureBrush);
        auto session = FocusSession(autofocusStartedTime, time, nullptr, false, autofocusStartedTemperature, filter, reason,
                                    reasonInfo, points, useWeights, "", "", failCode, failCodeInfo);
        focusSessions.add(session);
        updateMaxX(time);
        if (!batchMode)
        {
            if (runtimeDisplay && keepCurrentCB->isChecked() && statsCursor == nullptr)
                focusSessionClicked(session, false);
            replot();
        }
        autofocusStartedTime = -1;
    }
}

// Older version processAutofocusAborted to support processing analyze files created before V2
void Analyze::processAutofocusAborted(double time, const QString &filter, const QString &points, bool batchMode)
{
    removeTemporarySession(&temporaryFocusSession);
    double duration = time - autofocusStartedTime;
    if (autofocusStartedTime >= 0 && duration < 1000)
    {
        // Just in case..
        addSession(autofocusStartedTime, time, FOCUS_Y, failureBrush);
        auto session = FocusSession(autofocusStartedTime, time, nullptr, false,
                                    autofocusStartedTemperature, filter, points, "", "");
        focusSessions.add(session);
        updateMaxX(time);
        if (!batchMode)
        {
            if (runtimeDisplay && keepCurrentCB->isChecked() && statsCursor == nullptr)
                focusSessionClicked(session, false);
            replot();
        }
        autofocusStartedTime = -1;
    }
}

void Analyze::resetAutofocusState()
{
    autofocusStartedTime = -1;
    autofocusStartedFilter = "";
    autofocusStartedTemperature = 0;
    autofocusStartedReason = AutofocusReason::FOCUS_NONE;
    autofocusStartedReasonInfo = "";
}

namespace
{

// TODO: move to ekos.h/cpp?
Ekos::GuideState stringToGuideState(const QString &str)
{
    if (str == i18n("Idle"))
        return GUIDE_IDLE;
    else if (str == i18n("Aborted"))
        return GUIDE_ABORTED;
    else if (str == i18n("Connected"))
        return GUIDE_CONNECTED;
    else if (str == i18n("Disconnected"))
        return GUIDE_DISCONNECTED;
    else if (str == i18n("Capturing"))
        return GUIDE_CAPTURE;
    else if (str == i18n("Looping"))
        return GUIDE_LOOPING;
    else if (str == i18n("Subtracting"))
        return GUIDE_DARK;
    else if (str == i18n("Subframing"))
        return GUIDE_SUBFRAME;
    else if (str == i18n("Selecting star"))
        return GUIDE_STAR_SELECT;
    else if (str == i18n("Calibrating"))
        return GUIDE_CALIBRATING;
    else if (str == i18n("Calibration error"))
        return GUIDE_CALIBRATION_ERROR;
    else if (str == i18n("Calibrated"))
        return GUIDE_CALIBRATION_SUCCESS;
    else if (str == i18n("Guiding"))
        return GUIDE_GUIDING;
    else if (str == i18n("Suspended"))
        return GUIDE_SUSPENDED;
    else if (str == i18n("Reacquiring"))
        return GUIDE_REACQUIRE;
    else if (str == i18n("Dithering"))
        return GUIDE_DITHERING;
    else if (str == i18n("Manual Dithering"))
        return GUIDE_MANUAL_DITHERING;
    else if (str == i18n("Dithering error"))
        return GUIDE_DITHERING_ERROR;
    else if (str == i18n("Dithering successful"))
        return GUIDE_DITHERING_SUCCESS;
    else if (str == i18n("Settling"))
        return GUIDE_DITHERING_SETTLE;
    else
        return GUIDE_IDLE;
}

Analyze::SimpleGuideState convertGuideState(Ekos::GuideState state)
{
    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        case GUIDE_LOOPING:
            return Analyze::G_IDLE;
        case GUIDE_GUIDING:
            return Analyze::G_GUIDING;
        case GUIDE_CAPTURE:
        case GUIDE_DARK:
        case GUIDE_SUBFRAME:
        case GUIDE_STAR_SELECT:
            return Analyze::G_IGNORE;
        case GUIDE_CALIBRATING:
        case GUIDE_CALIBRATION_ERROR:
        case GUIDE_CALIBRATION_SUCCESS:
            return Analyze::G_CALIBRATING;
        case GUIDE_SUSPENDED:
        case GUIDE_REACQUIRE:
            return Analyze::G_SUSPENDED;
        case GUIDE_DITHERING:
        case GUIDE_MANUAL_DITHERING:
        case GUIDE_DITHERING_ERROR:
        case GUIDE_DITHERING_SUCCESS:
        case GUIDE_DITHERING_SETTLE:
            return Analyze::G_DITHERING;
    }
    // Shouldn't get here--would get compile error, I believe with a missing case.
    return Analyze::G_IDLE;
}

const QBrush guideBrush(Analyze::SimpleGuideState simpleState)
{
    switch (simpleState)
    {
        case Analyze::G_IDLE:
        case Analyze::G_IGNORE:
            // don't actually render these, so don't care.
            return offBrush;
        case Analyze::G_GUIDING:
            return successBrush;
        case Analyze::G_CALIBRATING:
            return progressBrush;
        case Analyze::G_SUSPENDED:
            return stoppedBrush;
        case Analyze::G_DITHERING:
            return progress2Brush;
    }
    // Shouldn't get here.
    return offBrush;
}

}  // namespace

void Analyze::guideState(Ekos::GuideState state)
{
    QString str = getGuideStatusString(state);
    saveMessage("GuideState", str);
    if (runtimeDisplay)
        processGuideState(logTime(), str);
}

void Analyze::processGuideState(double time, const QString &stateStr, bool batchMode)
{
    Ekos::GuideState gstate = stringToGuideState(stateStr);
    SimpleGuideState state = convertGuideState(gstate);
    if (state == G_IGNORE)
        return;
    if (state == lastGuideStateStarted)
        return;
    // End the previous guide session and start the new one.
    if (guideStateStartedTime >= 0)
    {
        if (lastGuideStateStarted != G_IDLE)
        {
            // Don't render the idle guiding
            addSession(guideStateStartedTime, time, GUIDE_Y, guideBrush(lastGuideStateStarted));
            guideSessions.add(GuideSession(guideStateStartedTime, time, nullptr, lastGuideStateStarted));
        }
    }
    if (state == G_GUIDING)
    {
        addTemporarySession(&temporaryGuideSession, time, 1, GUIDE_Y, successBrush);
        temporaryGuideSession.simpleState = state;
    }
    else
        removeTemporarySession(&temporaryGuideSession);

    guideStateStartedTime = time;
    lastGuideStateStarted = state;
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::resetGuideState()
{
    lastGuideStateStarted = G_IDLE;
    guideStateStartedTime = -1;
}

void Analyze::newTemperature(double temperatureDelta, double temperature)
{
    Q_UNUSED(temperatureDelta);
    if (temperature > -200 && temperature != lastTemperature)
    {
        saveMessage("Temperature", QString("%1").arg(QString::number(temperature, 'f', 3)));
        lastTemperature = temperature;
        if (runtimeDisplay)
            processTemperature(logTime(), temperature);
    }
}

void Analyze::processTemperature(double time, double temperature, bool batchMode)
{
    addTemperature(temperature, time);
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::resetTemperature()
{
    lastTemperature = -1000;
}

void Analyze::newTargetDistance(double targetDistance)
{
    saveMessage("TargetDistance", QString("%1").arg(QString::number(targetDistance, 'f', 0)));
    if (runtimeDisplay)
        processTargetDistance(logTime(), targetDistance);
}

void Analyze::processTargetDistance(double time, double targetDistance, bool batchMode)
{
    addTargetDistance(targetDistance, time);
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::guideStats(double raError, double decError, int raPulse, int decPulse,
                         double snr, double skyBg, int numStars)
{
    saveMessage("GuideStats", QString("%1,%2,%3,%4,%5,%6,%7")
                .arg(QString::number(raError, 'f', 3), QString::number(decError, 'f', 3))
                .arg(raPulse)
                .arg(decPulse)
                .arg(QString::number(snr, 'f', 3), QString::number(skyBg, 'f', 3))
                .arg(numStars));

    if (runtimeDisplay)
        processGuideStats(logTime(), raError, decError, raPulse, decPulse, snr, skyBg, numStars);
}

void Analyze::processGuideStats(double time, double raError, double decError,
                                int raPulse, int decPulse, double snr, double skyBg, int numStars, bool batchMode)
{
    addGuideStats(raError, decError, raPulse, decPulse, snr, numStars, skyBg, time);
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::resetGuideStats()
{
    lastGuideStatsTime = -1;
    lastCaptureRmsTime = -1;
    numStarsMax = 0;
    snrMax = 0;
    skyBgMax = 0;
}

namespace
{

// TODO: move to ekos.h/cpp
AlignState convertAlignState(const QString &str)
{
    for (int i = 0; i < alignStates.size(); ++i)
    {
        if (str == alignStates[i].toString())
            return static_cast<AlignState>(i);
    }
    return ALIGN_IDLE;
}

const QBrush alignBrush(AlignState state)
{
    switch (state)
    {
        case ALIGN_IDLE:
            return offBrush;
        case ALIGN_COMPLETE:
        case ALIGN_SUCCESSFUL:
            return successBrush;
        case ALIGN_FAILED:
            return failureBrush;
        case ALIGN_PROGRESS:
            return progress3Brush;
        case ALIGN_SYNCING:
            return progress2Brush;
        case ALIGN_SLEWING:
            return progressBrush;
        case ALIGN_ROTATING:
            return progress4Brush;
        case ALIGN_ABORTED:
            return failureBrush;
        case ALIGN_SUSPENDED:
            return offBrush;
    }
    // Shouldn't get here.
    return offBrush;
}
}  // namespace

void Analyze::alignState(AlignState state)
{
    if (state == lastAlignStateReceived)
        return;
    lastAlignStateReceived = state;

    QString stateStr = getAlignStatusString(state);
    saveMessage("AlignState", stateStr);
    if (runtimeDisplay)
        processAlignState(logTime(), stateStr);
}

//ALIGN_IDLE, ALIGN_COMPLETE, ALIGN_FAILED, ALIGN_ABORTED,ALIGN_PROGRESS,ALIGN_SYNCING,ALIGN_SLEWING
void Analyze::processAlignState(double time, const QString &statusString, bool batchMode)
{
    AlignState state = convertAlignState(statusString);

    if (state == lastAlignStateStarted)
        return;

    bool lastStateInteresting = (lastAlignStateStarted == ALIGN_PROGRESS ||
                                 lastAlignStateStarted == ALIGN_SYNCING ||
                                 lastAlignStateStarted == ALIGN_SLEWING);
    if (lastAlignStateStartedTime >= 0 && lastStateInteresting)
    {
        if (state == ALIGN_COMPLETE || state == ALIGN_FAILED || state == ALIGN_ABORTED)
        {
            // These states are really commetaries on the previous states.
            addSession(lastAlignStateStartedTime, time, ALIGN_Y, alignBrush(state));
            alignSessions.add(AlignSession(lastAlignStateStartedTime, time, nullptr, state));
        }
        else
        {
            addSession(lastAlignStateStartedTime, time, ALIGN_Y, alignBrush(lastAlignStateStarted));
            alignSessions.add(AlignSession(lastAlignStateStartedTime, time, nullptr, lastAlignStateStarted));
        }
    }
    bool stateInteresting = (state == ALIGN_PROGRESS || state == ALIGN_SYNCING ||
                             state == ALIGN_SLEWING);
    if (stateInteresting)
    {
        addTemporarySession(&temporaryAlignSession, time, 1, ALIGN_Y, temporaryBrush);
        temporaryAlignSession.state = state;
    }
    else
        removeTemporarySession(&temporaryAlignSession);

    lastAlignStateStartedTime = time;
    lastAlignStateStarted = state;
    updateMaxX(time);
    if (!batchMode)
        replot();

}

void Analyze::resetAlignState()
{
    lastAlignStateReceived = ALIGN_IDLE;
    lastAlignStateStarted = ALIGN_IDLE;
    lastAlignStateStartedTime = -1;
}

namespace
{

const QBrush mountBrush(ISD::Mount::Status state)
{
    switch (state)
    {
        case ISD::Mount::MOUNT_IDLE:
            return offBrush;
        case ISD::Mount::MOUNT_ERROR:
            return failureBrush;
        case ISD::Mount::MOUNT_MOVING:
        case ISD::Mount::MOUNT_SLEWING:
            return progressBrush;
        case ISD::Mount::MOUNT_TRACKING:
            return successBrush;
        case ISD::Mount::MOUNT_PARKING:
            return stoppedBrush;
        case ISD::Mount::MOUNT_PARKED:
            return stopped2Brush;
    }
    // Shouldn't get here.
    return offBrush;
}

}  // namespace

// Mount status can be:
// MOUNT_IDLE, MOUNT_MOVING, MOUNT_SLEWING, MOUNT_TRACKING, MOUNT_PARKING, MOUNT_PARKED, MOUNT_ERROR
void Analyze::mountState(ISD::Mount::Status state)
{
    QString statusString = mountStatusString(state);
    saveMessage("MountState", statusString);
    if (runtimeDisplay)
        processMountState(logTime(), statusString);
}

void Analyze::processMountState(double time, const QString &statusString, bool batchMode)
{
    ISD::Mount::Status state = toMountStatus(statusString);
    if (mountStateStartedTime >= 0 && lastMountState != ISD::Mount::MOUNT_IDLE)
    {
        addSession(mountStateStartedTime, time, MOUNT_Y, mountBrush(lastMountState));
        mountSessions.add(MountSession(mountStateStartedTime, time, nullptr, lastMountState));
    }

    if (state != ISD::Mount::MOUNT_IDLE)
    {
        addTemporarySession(&temporaryMountSession, time, 1, MOUNT_Y,
                            (state == ISD::Mount::MOUNT_TRACKING) ? successBrush : temporaryBrush);
        temporaryMountSession.state = state;
    }
    else
        removeTemporarySession(&temporaryMountSession);

    mountStateStartedTime = time;
    lastMountState = state;
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::resetMountState()
{
    mountStateStartedTime = -1;
    lastMountState = ISD::Mount::Status::MOUNT_IDLE;
}

// This message comes from the mount module
void Analyze::mountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &haValue)
{
    double ra = position.ra().Degrees();
    double dec = position.dec().Degrees();
    double ha = haValue.Degrees();
    double az = position.az().Degrees();
    double alt = position.alt().Degrees();

    // Only process the message if something's changed by 1/4 degree or more.
    constexpr double MIN_DEGREES_CHANGE = 0.25;
    if ((fabs(ra - lastMountRa) > MIN_DEGREES_CHANGE) ||
            (fabs(dec - lastMountDec) > MIN_DEGREES_CHANGE) ||
            (fabs(ha - lastMountHa) > MIN_DEGREES_CHANGE) ||
            (fabs(az - lastMountAz) > MIN_DEGREES_CHANGE) ||
            (fabs(alt - lastMountAlt) > MIN_DEGREES_CHANGE) ||
            (pierSide != lastMountPierSide))
    {
        saveMessage("MountCoords", QString("%1,%2,%3,%4,%5,%6")
                    .arg(QString::number(ra, 'f', 4), QString::number(dec, 'f', 4),
                         QString::number(az, 'f', 4), QString::number(alt, 'f', 4))
                    .arg(pierSide)
                    .arg(QString::number(ha, 'f', 4)));

        if (runtimeDisplay)
            processMountCoords(logTime(), ra, dec, az, alt, pierSide, ha);

        lastMountRa = ra;
        lastMountDec = dec;
        lastMountHa = ha;
        lastMountAz = az;
        lastMountAlt = alt;
        lastMountPierSide = pierSide;
    }
}

void Analyze::processMountCoords(double time, double ra, double dec, double az,
                                 double alt, int pierSide, double ha, bool batchMode)
{
    addMountCoords(ra, dec, az, alt, pierSide, ha, time);
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::resetMountCoords()
{
    lastMountRa = -1;
    lastMountDec = -1;
    lastMountHa = -1;
    lastMountAz = -1;
    lastMountAlt = -1;
    lastMountPierSide = -1;
}

namespace
{

// TODO: Move to mount.h/cpp?
MeridianFlipState::MeridianFlipMountState convertMountFlipState(const QString &statusStr)
{
    if (statusStr == "MOUNT_FLIP_NONE")
        return MeridianFlipState::MOUNT_FLIP_NONE;
    else if (statusStr == "MOUNT_FLIP_PLANNED")
        return MeridianFlipState::MOUNT_FLIP_PLANNED;
    else if (statusStr == "MOUNT_FLIP_WAITING")
        return MeridianFlipState::MOUNT_FLIP_WAITING;
    else if (statusStr == "MOUNT_FLIP_ACCEPTED")
        return MeridianFlipState::MOUNT_FLIP_ACCEPTED;
    else if (statusStr == "MOUNT_FLIP_RUNNING")
        return MeridianFlipState::MOUNT_FLIP_RUNNING;
    else if (statusStr == "MOUNT_FLIP_COMPLETED")
        return MeridianFlipState::MOUNT_FLIP_COMPLETED;
    else if (statusStr == "MOUNT_FLIP_ERROR")
        return MeridianFlipState::MOUNT_FLIP_ERROR;
    return MeridianFlipState::MOUNT_FLIP_ERROR;
}

QBrush mountFlipStateBrush(MeridianFlipState::MeridianFlipMountState state)
{
    switch (state)
    {
        case MeridianFlipState::MOUNT_FLIP_NONE:
            return offBrush;
        case MeridianFlipState::MOUNT_FLIP_PLANNED:
            return stoppedBrush;
        case MeridianFlipState::MOUNT_FLIP_WAITING:
            return stopped2Brush;
        case MeridianFlipState::MOUNT_FLIP_ACCEPTED:
            return progressBrush;
        case MeridianFlipState::MOUNT_FLIP_RUNNING:
            return progress2Brush;
        case MeridianFlipState::MOUNT_FLIP_COMPLETED:
            return successBrush;
        case MeridianFlipState::MOUNT_FLIP_ERROR:
            return failureBrush;
    }
    // Shouldn't get here.
    return offBrush;
}
}  // namespace

void Analyze::mountFlipStatus(MeridianFlipState::MeridianFlipMountState state)
{
    if (state == lastMountFlipStateReceived)
        return;
    lastMountFlipStateReceived = state;

    QString stateStr = MeridianFlipState::meridianFlipStatusString(state);
    saveMessage("MeridianFlipState", stateStr);
    if (runtimeDisplay)
        processMountFlipState(logTime(), stateStr);

}

// MeridianFlipState::MOUNT_FLIP_NONE MeridianFlipState::MOUNT_FLIP_PLANNED MeridianFlipState::MOUNT_FLIP_WAITING MeridianFlipState::MOUNT_FLIP_ACCEPTED MeridianFlipState::MOUNT_FLIP_RUNNING MeridianFlipState::MOUNT_FLIP_COMPLETED MeridianFlipState::MOUNT_FLIP_ERROR
void Analyze::processMountFlipState(double time, const QString &statusString, bool batchMode)
{
    MeridianFlipState::MeridianFlipMountState state = convertMountFlipState(statusString);
    if (state == lastMountFlipStateStarted)
        return;

    bool lastStateInteresting =
        (lastMountFlipStateStarted == MeridianFlipState::MOUNT_FLIP_PLANNED ||
         lastMountFlipStateStarted == MeridianFlipState::MOUNT_FLIP_WAITING ||
         lastMountFlipStateStarted == MeridianFlipState::MOUNT_FLIP_ACCEPTED ||
         lastMountFlipStateStarted == MeridianFlipState::MOUNT_FLIP_RUNNING);
    if (mountFlipStateStartedTime >= 0 && lastStateInteresting)
    {
        if (state == MeridianFlipState::MOUNT_FLIP_COMPLETED || state == MeridianFlipState::MOUNT_FLIP_ERROR)
        {
            // These states are really commentaries on the previous states.
            addSession(mountFlipStateStartedTime, time, MERIDIAN_MOUNT_FLIP_Y, mountFlipStateBrush(state));
            mountFlipSessions.add(MountFlipSession(mountFlipStateStartedTime, time, nullptr, state));
        }
        else
        {
            addSession(mountFlipStateStartedTime, time, MERIDIAN_MOUNT_FLIP_Y, mountFlipStateBrush(lastMountFlipStateStarted));
            mountFlipSessions.add(MountFlipSession(mountFlipStateStartedTime, time, nullptr, lastMountFlipStateStarted));
        }
    }
    bool stateInteresting =
        (state == MeridianFlipState::MOUNT_FLIP_PLANNED ||
         state == MeridianFlipState::MOUNT_FLIP_WAITING ||
         state == MeridianFlipState::MOUNT_FLIP_ACCEPTED ||
         state == MeridianFlipState::MOUNT_FLIP_RUNNING);
    if (stateInteresting)
    {
        addTemporarySession(&temporaryMountFlipSession, time, 1, MERIDIAN_MOUNT_FLIP_Y, temporaryBrush);
        temporaryMountFlipSession.state = state;
    }
    else
        removeTemporarySession(&temporaryMountFlipSession);

    mountFlipStateStartedTime = time;
    lastMountFlipStateStarted = state;
    updateMaxX(time);
    if (!batchMode)
        replot();
}

void Analyze::resetMountFlipState()
{
    lastMountFlipStateReceived = MeridianFlipState::MOUNT_FLIP_NONE;
    lastMountFlipStateStarted = MeridianFlipState::MOUNT_FLIP_NONE;
    mountFlipStateStartedTime = -1;
}

QBrush Analyze::schedulerJobBrush(const QString &jobName, bool temporary)
{
    QList<QColor> colors =
    {
        {110, 120, 150}, {150, 180, 180}, {180, 165, 130}, {180, 200, 140}, {250, 180, 130},
        {190, 170, 160}, {140, 110, 160}, {250, 240, 190}, {250, 200, 220}, {150, 125, 175}
    };

    Qt::BrushStyle pattern = temporary ? Qt::Dense4Pattern : Qt::SolidPattern;
    auto it = schedulerJobColors.constFind(jobName);
    if (it == schedulerJobColors.constEnd())
    {
        const int numSoFar = schedulerJobColors.size();
        auto color = colors[numSoFar % colors.size()];
        schedulerJobColors[jobName] = color;
        return QBrush(color, pattern);
    }
    else
    {
        return QBrush(*it, pattern);
    }
}

void Analyze::schedulerJobStarted(const QString &jobName)
{
    saveMessage("SchedulerJobStart", jobName);
    if (runtimeDisplay)
        processSchedulerJobStarted(logTime(), jobName);

}

void Analyze::schedulerJobEnded(const QString &jobName, const QString &reason)
{
    saveMessage("SchedulerJobEnd", QString("%1,%2").arg(jobName, reason));
    if (runtimeDisplay)
        processSchedulerJobEnded(logTime(), jobName, reason);
}


// Called by either the above (when live data is received), or reading from file.
// BatchMode would be true when reading from file.
void Analyze::processSchedulerJobStarted(double time, const QString &jobName)
{
    checkForMissingSchedulerJobEnd(time - 1);
    schedulerJobStartedTime = time;
    schedulerJobStartedJobName = jobName;
    updateMaxX(time);

    addTemporarySession(&temporarySchedulerJobSession, time, 1, SCHEDULER_Y, schedulerJobBrush(jobName, true));
    temporarySchedulerJobSession.jobName = jobName;
}

// Called when the captureComplete slot receives a signal.
void Analyze::processSchedulerJobEnded(double time, const QString &jobName, const QString &reason, bool batchMode)
{
    removeTemporarySession(&temporarySchedulerJobSession);

    if (schedulerJobStartedTime < 0)
    {
        replot();
        return;
    }

    addSession(schedulerJobStartedTime, time, SCHEDULER_Y, schedulerJobBrush(jobName, false));
    auto session = SchedulerJobSession(schedulerJobStartedTime, time, nullptr, jobName, reason);
    schedulerJobSessions.add(session);
    updateMaxX(time);
    resetSchedulerJob();
    if (!batchMode)
        replot();
}

// Just called in batch mode, in case the processSchedulerJobEnded was never called.
void Analyze::checkForMissingSchedulerJobEnd(double time)
{
    if (schedulerJobStartedTime < 0)
        return;
    removeTemporarySession(&temporarySchedulerJobSession);
    addSession(schedulerJobStartedTime, time, SCHEDULER_Y, schedulerJobBrush(schedulerJobStartedJobName, false));
    auto session = SchedulerJobSession(schedulerJobStartedTime, time, nullptr, schedulerJobStartedJobName, "missing job end");
    schedulerJobSessions.add(session);
    updateMaxX(time);
    resetSchedulerJob();
}

void Analyze::resetSchedulerJob()
{
    schedulerJobStartedTime = -1;
    schedulerJobStartedJobName = "";
}

void Analyze::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_ANALYZE) << text;

    emit newLog(text);
}

void Analyze::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

}  // namespace Ekos
