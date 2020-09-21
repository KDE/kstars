/*  Ekos Analyze
    Copyright (C) 2020 Hy Murveit <hy@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "analyze.h"

#include <KNotifications/KNotification>
#include <QDateTime>
#include <QShortcut>
#include <QtGlobal>

#include "auxiliary/kspaths.h"
#include "dms.h"
#include "ekos/manager.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsviewer.h"
#include "ksmessagebox.h"
#include "kstars.h"
#include "Options.h"

#include <ekos_analyze_debug.h>
#include <KHelpClient>
#include <version.h>

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
int NUMSTARS_GRAPH = -1;
int SKYBG_GRAPH = -1;
int SNR_GRAPH = -1;
int RA_GRAPH = -1;
int DEC_GRAPH = -1;
int RA_PULSE_GRAPH = -1;
int DEC_PULSE_GRAPH = -1;
int DRIFT_GRAPH = -1;
int RMS_GRAPH = -1;
int MOUNT_RA_GRAPH = -1;
int MOUNT_DEC_GRAPH = -1;
int MOUNT_HA_GRAPH = -1;
int AZ_GRAPH = -1;
int ALT_GRAPH = -1;
int PIER_SIDE_GRAPH = -1;

// Initialized in initGraphicsPlot().
int FOCUS_GRAPHICS = -1;
int FOCUS_GRAPHICS_FINAL = -1;
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
const QString mountStatusString(ISD::Telescope::Status status)
{
    switch (status)
    {
        case ISD::Telescope::MOUNT_IDLE:
            return i18n("Idle");
        case ISD::Telescope::MOUNT_PARKED:
            return i18n("Parked");
        case ISD::Telescope::MOUNT_PARKING:
            return i18n("Parking");
        case ISD::Telescope::MOUNT_SLEWING:
            return i18n("Slewing");
        case ISD::Telescope::MOUNT_MOVING:
            return i18n("Moving");
        case ISD::Telescope::MOUNT_TRACKING:
            return i18n("Tracking");
        case ISD::Telescope::MOUNT_ERROR:
            return i18n("Error");
    }
    return i18n("Error");
}

ISD::Telescope::Status toMountStatus(const QString &str)
{
    if (str == i18n("Idle"))
        return ISD::Telescope::MOUNT_IDLE;
    else if (str == i18n("Parked"))
        return ISD::Telescope::MOUNT_PARKED;
    else if (str == i18n("Parking"))
        return ISD::Telescope::MOUNT_PARKING;
    else if (str == i18n("Slewing"))
        return ISD::Telescope::MOUNT_SLEWING;
    else if (str == i18n("Moving"))
        return ISD::Telescope::MOUNT_MOVING;
    else if (str == i18n("Tracking"))
        return ISD::Telescope::MOUNT_TRACKING;
    else
        return ISD::Telescope::MOUNT_ERROR;
}

// Returns the stripe color used when drawing the capture timeline for various filters.
// TODO: Not sure how to internationalize this.
bool filterStripeBrush(const QString &filter, QBrush *brush)
{
    if (!filter.compare("red", Qt::CaseInsensitive) ||
            !filter.compare("r", Qt::CaseInsensitive))
    {
        *brush = QBrush(Qt::red, Qt::SolidPattern);
        return true;
    }
    else if (!filter.compare("green", Qt::CaseInsensitive) ||
             !filter.compare("g", Qt::CaseInsensitive))
    {
        *brush = QBrush(Qt::green, Qt::SolidPattern);
        return true;
    }
    else if (!filter.compare("blue", Qt::CaseInsensitive) ||
             !filter.compare("b", Qt::CaseInsensitive))
    {
        *brush = QBrush(Qt::blue, Qt::SolidPattern);
        return true;
    }
    else if (!filter.compare("ha", Qt::CaseInsensitive) ||
             !filter.compare("h", Qt::CaseInsensitive) ||
             !filter.compare("h_alpha", Qt::CaseInsensitive) ||
             !filter.compare("halpha", Qt::CaseInsensitive))
    {
        *brush = QBrush(Qt::darkRed, Qt::SolidPattern);
        return true;
    }
    else if (!filter.compare("oiii", Qt::CaseInsensitive) ||
             !filter.compare("o3", Qt::CaseInsensitive))
    {
        *brush = QBrush(Qt::cyan, Qt::SolidPattern);
        return true;
    }
    else if (!filter.compare("sii", Qt::CaseInsensitive) ||
             !filter.compare("s2", Qt::CaseInsensitive))
    {
        // Pink.
        *brush = QBrush(QColor(255, 182, 193), Qt::SolidPattern);
        return true;
    }
    else if (!filter.compare("lpr", Qt::CaseInsensitive) ||
             !filter.compare("L", Qt::CaseInsensitive) ||
             !filter.compare("luminance", Qt::CaseInsensitive) ||
             !filter.compare("lum", Qt::CaseInsensitive) ||
             !filter.compare("lps", Qt::CaseInsensitive) ||
             !filter.compare("cls", Qt::CaseInsensitive))
    {
        *brush = QBrush(Qt::white, Qt::SolidPattern);
        return true;
    }
    return false;
}

// Used when searching for FITS files to display.
// If filename isn't found as is, it tries alterateDirectory in several ways
// e.g. if filename = /1/2/3/4/name is not found, then try alternateDirectory/name,
// then alternateDirectory/4/name, then alternateDirectory/3/4/name,
// then alternateDirectory/2/3/4/name, and so on.
// If it cannot find the FITS file, it returns an empty string, otherwise it returns
// the full path where the file was found.
QString findFilename(const QString &filename, const QString &alternateDirectory)
{
    // Try the origial full path.
    QFileInfo info(filename);
    if (info.exists() && info.isFile())
        return filename;

    // Try putting the filename at the end of the full path onto alternateDirectory.
    QString name = info.fileName();
    QString temp = QString("%1/%2").arg(alternateDirectory).arg(name);
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

        QString temp2 = QString("%1%2").arg(alternateDirectory).arg(filename.right(size - index));
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
            for (const auto i : intervals)
            {
                if (t >= i.start && t <= i.end)
                    result.push_back(i);
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

}  // namespace

namespace Ekos
{

Analyze::Analyze()
{
    setupUi(this);

    initRmsFilter();

    alternateFolder = QDir::homePath();

    initInputSelection();
    initTimelinePlot();
    initStatsPlot();
    initGraphicsPlot();
    fullWidthCB->setChecked(true);
    runtimeDisplay = true;
    fullWidthCB->setVisible(true);
    fullWidthCB->setDisabled(false);
    connect(fullWidthCB, &QCheckBox::toggled, [ = ](bool checked)
    {
        if (checked)
            this->replot();
    });

    initStatsCheckboxes();

    connect(zoomInB, &QPushButton::clicked, this, &Ekos::Analyze::zoomIn);
    connect(zoomOutB, &QPushButton::clicked, this, &Ekos::Analyze::zoomOut);
    connect(timelinePlot, &QCustomPlot::mousePress, this, &Ekos::Analyze::timelineMousePress);
    connect(timelinePlot, &QCustomPlot::mouseDoubleClick, this, &Ekos::Analyze::timelineMouseDoubleClick);
    connect(timelinePlot, &QCustomPlot::mouseWheel, this, &Ekos::Analyze::timelineMouseWheel);
    connect(statsPlot, &QCustomPlot::mousePress, this, &Ekos::Analyze::statsMousePress);
    connect(statsPlot, &QCustomPlot::mouseDoubleClick, this, &Ekos::Analyze::statsMouseDoubleClick);
    connect(statsPlot, &QCustomPlot::mouseMove, this, &Ekos::Analyze::statsMouseMove);
    connect(analyzeSB, &QScrollBar::valueChanged, this, &Ekos::Analyze::scroll);
    analyzeSB->setRange(0, MAX_SCROLL_VALUE);
    connect(helpB, &QPushButton::clicked, this, &Ekos::Analyze::helpMessage);
    connect(keepCurrentCB, &QCheckBox::stateChanged, this, &Ekos::Analyze::keepCurrent);

    setupKeyboardShortcuts(timelinePlot);

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

// Implements the input selection UI. User can either choose the current Ekos
// session, or a file read from disk, or set the alternateDirectory variable.
void Analyze::initInputSelection()
{
    // Setup the input combo box.
    dirPath = QUrl::fromLocalFile(KSPaths::writableLocation(
                                      QStandardPaths::GenericDataLocation) + "analyze/");

    inputCombo->addItem(i18n("Current Session"));
    inputCombo->addItem(i18n("Read from File"));
    inputCombo->addItem(i18n("Set alternative image-file base directory"));
    inputValue->setText("");
    connect(inputCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&](int index)
    {
        if (index == 0)
        {
            // Input from current session
            if (!runtimeDisplay)
            {
                reset();
                inputValue->setText(i18n("Current Session"));
                maxXValue = readDataFromFile(logFilename);
                runtimeDisplay = true;
            }
            fullWidthCB->setChecked(true);
            fullWidthCB->setVisible(true);
            fullWidthCB->setDisabled(false);
            replot();
        }
        else if (index == 1)
        {
            // Input from a file.
            QUrl inputURL = QFileDialog::getOpenFileUrl(this, i18n("Select input file"), dirPath,
                            i18n("Analyze Log (*.analyze);;All Files (*)"));
            if (inputURL.isEmpty())
                return;
            dirPath = QUrl(inputURL.url(QUrl::RemoveFilename));

            reset();
            inputValue->setText(inputURL.fileName());

            // If we do this after the readData call below, it would animate the sequence.
            runtimeDisplay = false;

            maxXValue = readDataFromFile(inputURL.toLocalFile());
            plotStart = 0;
            plotWidth = maxXValue + 5;
            replot();
        }
        else if (index == 2)
        {
            QString dir = QFileDialog::getExistingDirectory(
                              this, i18n("Set an alternate base directory for your captured images"),
                              QDir::homePath(),
                              QFileDialog::ShowDirsOnly);
            if (dir.size() > 0)
            {
                // TODO: replace with an option.
                alternateFolder = dir;
            }
            // This is not a destiation, reset to one of the above.
            if (runtimeDisplay)
                inputCombo->setCurrentIndex(0);
            else
                inputCombo->setCurrentIndex(1);
        }
    });
}

void Analyze::setupKeyboardShortcuts(QCustomPlot *plot)
{
    // Shortcuts defined: https://doc.qt.io/archives/qt-4.8/qkeysequence.html#standard-shortcuts
    QShortcut *s = new QShortcut(QKeySequence(QKeySequence::ZoomIn), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::zoomIn);
    s = new QShortcut(QKeySequence(QKeySequence::ZoomOut), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::zoomOut);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToNextChar), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::scrollRight);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousChar), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::scrollLeft);
    s = new QShortcut(QKeySequence("?"), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::helpMessage);
    s = new QShortcut(QKeySequence("h"), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::helpMessage);
    s = new QShortcut(QKeySequence(QKeySequence::HelpContents), plot);
    connect(s, &QShortcut::activated, this, &Ekos::Analyze::helpMessage);
}

Analyze::~Analyze()
{
    // TODO:
    // We should write out to disk any sessions that haven't terminated
    // (e.g. capture, focus, guide)
}

// When a user selects a timeline session, the previously selected one
// is deselected.  Note: this does not replot().
void Analyze::unhighlightTimelineItem()
{
    if (selectionHighlight != nullptr)
    {
        timelinePlot->removeItem(selectionHighlight);
        selectionHighlight = nullptr;
    }
    infoBox->clear();
}

// Highlight the area between start and end on row y in Timeline.
// Note that this doesn't replot().
void Analyze::highlightTimelineItem(double y, double start, double end)
{
    constexpr double halfHeight = 0.5;
    unhighlightTimelineItem();

    QCPItemRect *rect = new QCPItemRect(timelinePlot);
    rect->topLeft->setCoords(start, y + halfHeight);
    rect->bottomRight->setCoords(end, y - halfHeight);
    rect->setBrush(timelineSelectionBrush);
    selectionHighlight = rect;
}

// These help calculate the RMS values of the ra and drift errors. It takes
// an approximate moving average of the squared errors roughly averaged over
// 40 samples implemented by a simple digital low-pass filter.
void Analyze::initRmsFilter()
{
    constexpr double timeConstant = 40.0;
    rmsFilterAlpha = 1.0 / pow(timeConstant, 0.865);
}

double Analyze::rmsFilter(double x)
{
    filteredRMS = rmsFilterAlpha * x + (1.0 - rmsFilterAlpha) * filteredRMS;
    return filteredRMS;
}

void Analyze::resetRmsFilter()
{
    filteredRMS = 0;
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
        resetRmsFilter();
    }

    const double drift = std::hypot(raDrift, decDrift);

    // To compute the RMS error, which is sqrt(sum square error / N), filter the squared
    // error, which effectively returns sum squared error / N.
    // The RMS value is then the sqrt of the filtered value.
    const double rms = sqrt(rmsFilter(raDrift * raDrift + decDrift * decDrift));

    addGuideStatsInternal(raDrift, decDrift, double(raPulse), double(decPulse), snr, numStars, skyBackground, drift, rms, time);
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

    snrAxis->setRange(-1.05 * snrMax, std::max(10.0, 1.05 * snrMax));
    skyBgAxis->setRange(0, std::max(10.0, 1.15 * skyBgMax));
    numStarsAxis->setRange(0, std::max(10.0, 1.25 * numStarsMax));

    statsPlot->graph(SNR_GRAPH)->addData(time, snr);
    statsPlot->graph(NUMSTARS_GRAPH)->addData(time, numStars);
    statsPlot->graph(SKYBG_GRAPH)->addData(time, skyBackground);
}

// Add the HFR values to the Stats graph, as a constant value between startTime and time.
void Analyze::addHFR(double hfr, double time, double startTime)
{
    // The HFR corresponds to the last capture
    statsPlot->graph(HFR_GRAPH)->addData(startTime - .0001, qQNaN());
    statsPlot->graph(HFR_GRAPH)->addData(startTime, hfr);
    statsPlot->graph(HFR_GRAPH)->addData(time, hfr);
    statsPlot->graph(HFR_GRAPH)->addData(time + .0001, qQNaN());
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
        processCaptureStarting(time, exposureSeconds, filter, true);
    }
    else if ((list[0] == "CaptureComplete") && (list.size() == 6))
    {
        const double exposureSeconds = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        const QString filter = list[3];
        const double hfr = QString(list[4]).toDouble(&ok);
        if (!ok)
            return 0;
        const QString filename = list[5];
        processCaptureComplete(time, filename, exposureSeconds, filter, hfr, true);
    }
    else if ((list[0] == "CaptureAborted") && (list.size() == 3))
    {
        const double exposureSeconds = QString(list[2]).toDouble(&ok);
        if (!ok)
            return 0;
        processCaptureAborted(time, exposureSeconds, true);
    }
    else if ((list[0] == "AutofocusStarting") && (list.size() == 4))
    {
        QString filter = list[2];
        double temperature = QString(list[3]).toDouble(&ok);
        if (!ok)
            return 0;
        processAutofocusStarting(time, temperature, filter, true);
    }
    else if ((list[0] == "AutofocusComplete") && (list.size() == 4))
    {
        QString filter = list[2];
        QString samples = list[3];
        processAutofocusComplete(time, filter, samples, true);
    }
    else if ((list[0] == "AutofocusAborted") && (list.size() == 4))
    {
        QString filter = list[2];
        QString samples = list[3];
        processAutofocusAborted(time, filter, samples, true);
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
    else
    {
        return 0;
    }
    return time;
}

// Helper to create tables in the infoBox display.
// Start the table, displaying the heading and timing information, common to all sessions.
void Analyze::Session::startTable(const QString &name, const QString &status,
                                  const QDateTime &startClock, const QDateTime &endClock)
{
    QString startDateStr = startClock.toString("dd.MM.yyyy");
    QString startTimeStr = startClock.toString("hh:mm:ss");
    QString endTimeStr = isTemporary() ? "Ongoing"
                         : endClock.toString("hh:mm:ss");

    htmlString = QString("<style>td { padding: 0px 10px }</style>"
                         "<html><table><tr style=\"color:yellow\"><td>%1</td><td>%2</td></tr>"
                         "<tr><td style=\"color:yellow\">Date</td><td>%3</td><td></td></tr>"
                         "<tr><td style=\"color:yellow\">Interval</td><td>%4s</td><td>%5s</td></tr>"
                         "<tr><td style=\"color:yellow\">Clock</td><td>%6</td><td>%7</td></tr>"
                         "<tr><td style=\"color:yellow\">Duration</td><td>%8s</td><td></td></tr>")
                 .arg(name).arg(status)
                 .arg(startDateStr)
                 .arg(QString::number(start, 'f', 3))
                 .arg(isTemporary() ? "Ongoing" : QString::number(end, 'f', 3))
                 .arg(startTimeStr).arg(endTimeStr)
                 .arg(QString::number(end - start, 'f', 1));
}

// Add a new row to the table, which is specific to the particular Timeline line.
void Analyze::Session::addRow(const QString &key, const QString &value1String, const QString &value2String)
{

    QString row;
    if (value2String.size() == 0)
        // When the 2nd value is empty, have the 1st span both columns.
        row = QString("<tr><td style=\"color:yellow\">%1</td><td colspan=\"2\">%2</td></tr>")
              .arg(key).arg(value1String).arg(value2String);
    else
        row = QString("<tr><td style=\"color:yellow\">%1</td><td>%2</td><td>%3</td></tr>")
              .arg(key).arg(value1String).arg(value2String);
    htmlString = htmlString + row;
}

// Complete the table.
QString Analyze::Session::html() const
{
    return htmlString + "</table></html>";
}

bool Analyze::Session::isTemporary() const
{
    return rect != nullptr;
}

// The focus session parses the "pipe-separate-values" list of positions
// and HFRs given it, eventually to be used to plot the focus v-curve.
Analyze::FocusSession::FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                                    const QString &filter_, const QString &points_)
    : Session(start_, end_, FOCUS_Y, rect), success(ok),
      temperature(temperature_), filter(filter_), points(points_)
{
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
            fprintf(stderr, "Bad focus position %d in %s\n", i - 2, points.toLatin1().data());
            return;
        }
        positions.push_back(position);
        hfrs.push_back(hfr);
    }
}

// When the user clicks on a particular capture session in the timeline,
// a table is rendered in the infoBox, and, if it was a double click,
// the fits file is displayed, if it can be found.
void Analyze::captureSessionClicked(CaptureSession &c, bool doubleClick)
{
    highlightTimelineItem(c.offset, c.start, c.end);

    if (c.isTemporary())
        c.startTable("Capture", "in progress", clockTime(c.start), clockTime(c.start));
    else if (c.aborted)
        c.startTable("Capture", "ABORTED", clockTime(c.start), clockTime(c.end));
    else
        c.startTable("Capture", "successful", clockTime(c.start), clockTime(c.end));

    c.addRow("Filter", c.filter);

    double raRMS, decRMS, totalRMS;
    int numSamples;
    displayGuideGraphics(c.start, c.end, &raRMS, &decRMS, &totalRMS, &numSamples);
    if (numSamples > 0)
        c.addRow("GuideRMS", QString::number(totalRMS, 'f', 2));

    c.addRow("Exposure", QString::number(c.duration, 'f', 2));
    if (!c.isTemporary())
        c.addRow("Filename", c.filename);
    infoBox->setHtml(c.html());

    if (doubleClick && !c.isTemporary())
    {
        QString filename = findFilename(c.filename, alternateFolder);
        if (filename.size() > 0)
            displayFITS(filename);
        else
        {
            QString message = i18n("Could not find image file: %1", c.filename);
            KSNotification::sorry(message, i18n("Invalid URL"));
        }
    }
}

// When the user clicks on a focus session in the timeline,
// a table is rendered in the infoBox, and the HFR/position plot
// is displayed in the graphics plot. If focus is ongoing
// the information for the graphics is not plotted as it is not yet available.
void Analyze::focusSessionClicked(FocusSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(c.offset, c.start, c.end);

    if (c.success)
        c.startTable("Focus", "successful", clockTime(c.start), clockTime(c.end));
    else if (c.isTemporary())
        c.startTable("Focus", "in progress", clockTime(c.start), clockTime(c.start));
    else
        c.startTable("Focus", "FAILED", clockTime(c.start), clockTime(c.end));

    double finalHfr = c.hfrs.size() > 0 ? c.hfrs.last() : 0;
    if (c.success)
        c.addRow("HFR", c.isTemporary() ? "" : QString::number(finalHfr, 'f', 2));
    if (!c.isTemporary())
        c.addRow("Iterations", QString::number(c.positions.size()));
    c.addRow("Filter", c.filter);
    c.addRow("Temperature", QString::number(c.temperature, 'f', 1));
    infoBox->setHtml(c.html());

    if (c.isTemporary())
        resetGraphicsPlot();
    else
        displayFocusGraphics(c.positions, c.hfrs, c.success);
}

// When the user clicks on a guide session in the timeline,
// a table is rendered in the infoBox. If it has a G_GUIDING state
// then a drift plot is generated and  RMS values are calculated
// for the guiding session's time interval.
void Analyze::guideSessionClicked(GuideSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(GUIDE_Y, c.start, c.end);

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

    c.startTable("Guide", st, clockTime(c.start), clockTime(c.end));
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
    infoBox->setHtml(c.html());
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
// a table is rendered in the infoBox.
void Analyze::mountSessionClicked(MountSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(MOUNT_Y, c.start, c.end);

    c.startTable("Mount", mountStatusString(c.state), clockTime(c.start),
                 clockTime(c.isTemporary() ? c.start : c.end));
    infoBox->setHtml(c.html());
}

// When the user clicks on a particular align session in the timeline,
// a table is rendered in the infoBox.
void Analyze::alignSessionClicked(AlignSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(ALIGN_Y, c.start, c.end);
    c.startTable("Align", getAlignStatusString(c.state), clockTime(c.start),
                 clockTime(c.isTemporary() ? c.start : c.end));
    infoBox->setHtml(c.html());
}

// When the user clicks on a particular meridian flip session in the timeline,
// a table is rendered in the infoBox.
void Analyze::mountFlipSessionClicked(MountFlipSession &c, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    highlightTimelineItem(MERIDIAN_FLIP_Y, c.start, c.end);
    c.startTable("Meridian Flip", Mount::meridianFlipStatusString(c.state),
                 clockTime(c.start), clockTime(c.isTemporary() ? c.start : c.end));
    infoBox->setHtml(c.html());
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
    else if (yval >= MERIDIAN_FLIP_Y - 0.5 && yval <= MERIDIAN_FLIP_Y + 0.5)
    {
        QList<MountFlipSession> candidates = mountFlipSessions.find(xval);
        if (candidates.size() > 0)
            mountFlipSessionClicked(candidates[0], doubleClick);
        else if ((temporaryMountFlipSession.rect != nullptr) &&
                 (xval > temporaryMountFlipSession.start))
            mountFlipSessionClicked(temporaryMountFlipSession, doubleClick);
    }
    setStatsCursor(xval);
    replot();
}

void Analyze::setStatsCursor(double time)
{
    removeStatsCursor();
    QCPItemLine *line = new QCPItemLine(statsPlot);
    line->setPen(QPen(Qt::darkGray, 1, Qt::SolidLine));
    double top = statsPlot->yAxis->range().upper;
    line->start->setCoords(time, 0);
    line->end->setCoords(time, top);
    statsCursor = line;
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
    cursorTimeOut->setText("");
    cursorClockTimeOut->setText("");
    statsCursorTime = -1;
}

// When the users clicks in the stats plot, the cursor is set at the corresponding time.
void Analyze::processStatsClick(QMouseEvent *event, bool doubleClick)
{
    Q_UNUSED(doubleClick);
    double xval = statsPlot->xAxis->pixelToCoord(event->x());
    if (event->button() == Qt::RightButton || event->modifiers() == Qt::ControlModifier)
        // Resets the range. Replot will take care of ra/dec needing negative values.
        statsPlot->yAxis->setRange(0, 5);
    else
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
    // If we're on the legend, adjust the y-axis.
    if (statsPlot->xAxis->pixelToCoord(event->x()) < plotStart)
    {
        yAxisInitialPos = statsPlot->yAxis->pixelToCoord(event->y());
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
    // If we're on the legend, adjust the y-axis.
    if (statsPlot->xAxis->pixelToCoord(event->x()) < plotStart)
    {
        auto range = statsPlot->yAxis->range();
        double yDiff = yAxisInitialPos - statsPlot->yAxis->pixelToCoord(event->y());
        statsPlot->yAxis->setRange(range.lower + yDiff, range.upper + yDiff);
        replot();
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
    plotStart = std::min(maxXValue - plotWidth, plotStart + plotWidth);
    fullWidthCB->setChecked(false);
    replot();

}
void Analyze::scrollLeft()
{
    plotStart = std::max(0.0, plotStart - plotWidth);
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

    // Don't reset the range if the user has changed it.
    auto yRange = statsPlot->yAxis->range();
    if ((yRange.lower == 0 || yRange.lower == -2) && (yRange.upper == 5))
    {
        // Only need negative numbers on the stats plot if we're plotting RA or DEC
        if (raCB->isChecked() || decCB->isChecked() || raPulseCB->isChecked() || decPulseCB->isChecked())
            statsPlot->yAxis->setRange(-2, 5);
        else
            statsPlot->yAxis->setRange(0, 5);
    }

    dateTicker->setOffset(displayStartTime.toMSecsSinceEpoch() / 1000.0);

    timelinePlot->replot();
    statsPlot->replot();
    graphicsPlot->replot();
    updateStatsValues();
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
    // HFR is the only one to use the last real value, that is, it
    // keeps the hfr from the last exposure.
    updateStat(time, hfrOut, statsPlot->graph(HFR_GRAPH), d2Fcn, true);
    updateStat(time, skyBgOut, statsPlot->graph(SKYBG_GRAPH), d2Fcn);
    updateStat(time, snrOut, statsPlot->graph(SNR_GRAPH), d2Fcn);
    updateStat(time, raOut, statsPlot->graph(RA_GRAPH), d2Fcn);
    updateStat(time, decOut, statsPlot->graph(DEC_GRAPH), d2Fcn);
    updateStat(time, driftOut, statsPlot->graph(DRIFT_GRAPH), d2Fcn);
    updateStat(time, rmsOut, statsPlot->graph(RMS_GRAPH), d2Fcn);
    updateStat(time, azOut, statsPlot->graph(AZ_GRAPH), d2Fcn);
    updateStat(time, altOut, statsPlot->graph(ALT_GRAPH), d2Fcn);

    auto hmsFcn = [](double d) -> QString { dms ra; ra.setD(d); return ra.toHMSString(); };
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
        return QString("%1%2h %3m").arg(sgn).arg(ha.hour(), 2, 10, z)
        .arg(ha.minute(), 2, 10, z);
    };
    updateStat(time, mountHaOut, statsPlot->graph(MOUNT_HA_GRAPH), haFcn);

    auto intFcn = [](double d) -> QString { return QString::number(d, 'f', 0); };
    updateStat(time, numStarsOut, statsPlot->graph(NUMSTARS_GRAPH), intFcn);
    updateStat(time, raPulseOut, statsPlot->graph(RA_PULSE_GRAPH), intFcn);
    updateStat(time, decPulseOut, statsPlot->graph(DEC_PULSE_GRAPH), intFcn);

    auto pierFcn = [](double d) -> QString
    {
        return d == 0.0 ? "W (ptg E)" : d == 1.0 ? "E (ptg W)" : "?";
    };
    updateStat(time, pierSideOut, statsPlot->graph(PIER_SIDE_GRAPH), pierFcn);
}

void Analyze::initStatsCheckboxes()
{
    hfrCB->setChecked(Options::analyzeHFR());
    numStarsCB->setChecked(Options::analyzeNumStars());
    skyBgCB->setChecked(Options::analyzeSkyBg());
    snrCB->setChecked(Options::analyzeSNR());
    raCB->setChecked(Options::analyzeRA());
    decCB->setChecked(Options::analyzeDEC());
    raPulseCB->setChecked(Options::analyzeRAp());
    decPulseCB->setChecked(Options::analyzeDECp());
    driftCB->setChecked(Options::analyzeDrift());
    rmsCB->setChecked(Options::analyzeRMS());
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
// Generic initialization of a plot, applied to all plots in this tab.
void initQCP(QCustomPlot *plot)
{
    plot->setBackground(QBrush(Qt::black));
    plot->xAxis->setBasePen(QPen(Qt::white, 1));
    plot->yAxis->setBasePen(QPen(Qt::white, 1));
    plot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140, 140), 1, Qt::DotLine));
    plot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140, 140), 1, Qt::DotLine));
    plot->xAxis->grid()->setSubGridPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
    plot->yAxis->grid()->setSubGridPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
    plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    plot->yAxis->grid()->setZeroLinePen(QPen(Qt::white, 1));
    plot->xAxis->setBasePen(QPen(Qt::white, 1));
    plot->yAxis->setBasePen(QPen(Qt::white, 1));
    plot->xAxis->setTickPen(QPen(Qt::white, 1));
    plot->yAxis->setTickPen(QPen(Qt::white, 1));
    plot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    plot->yAxis->setSubTickPen(QPen(Qt::white, 1));
    plot->xAxis->setTickLabelColor(Qt::white);
    plot->yAxis->setTickLabelColor(Qt::white);
    plot->xAxis->setLabelColor(Qt::white);
    plot->yAxis->setLabelColor(Qt::white);
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
    textTicker->addTick(MERIDIAN_FLIP_Y, i18n("Flip"));
    textTicker->addTick(MOUNT_Y, i18n("Mount"));
    timelinePlot->yAxis->setTicker(textTicker);
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

int Analyze::initGraph(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                       const QColor &color, const QString &name)
{
    int num = plot->graphCount();
    plot->addGraph(plot->xAxis, yAxis);
    plot->graph(num)->setLineStyle(lineStyle);
    plot->graph(num)->setPen(QPen(color));
    plot->graph(num)->setName(name);
    return num;
}

template <typename Func>
int Analyze::initGraphAndCB(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                            const QColor &color, const QString &name, QCheckBox *cb, Func setCb)

{
    const int num = initGraph(plot, yAxis, lineStyle, color, name);
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

void Analyze::initStatsPlot()
{
    initQCP(statsPlot);

    // Setup the legend
    statsPlot->legend->setVisible(true);
    statsPlot->legend->setFont(QFont("Helvetica", 6));
    statsPlot->legend->setTextColor(Qt::white);
    // Legend background is black and ~75% opaque.
    statsPlot->legend->setBrush(QBrush(QColor(0, 0, 0, 190)));
    // Legend stacks vertically.
    statsPlot->legend->setFillOrder(QCPLegend::foRowsFirst);
    // Rows pretty tightly packed.
    statsPlot->legend->setRowSpacing(-3);
    statsPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft | Qt::AlignTop);

    // Add the graphs.

    HFR_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsStepRight, Qt::cyan, "HFR", hfrCB,
                               Options::setAnalyzeHFR);
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

    numStarsAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    numStarsAxis->setVisible(false);
    numStarsAxis->setRange(0, 15000);
    NUMSTARS_GRAPH = initGraphAndCB(statsPlot, numStarsAxis, QCPGraph::lsStepRight, Qt::magenta, "#Stars", numStarsCB,
                                    Options::setAnalyzeNumStars);

    skyBgAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    skyBgAxis->setVisible(false);
    skyBgAxis->setRange(0, 1000);
    SKYBG_GRAPH = initGraphAndCB(statsPlot, skyBgAxis, QCPGraph::lsStepRight, Qt::darkYellow, "SkyBG", skyBgCB,
                                 Options::setAnalyzeSkyBg);

    snrAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    snrAxis->setVisible(false);
    snrAxis->setRange(-100, 100);  // this will be reset.
    SNR_GRAPH = initGraphAndCB(statsPlot, snrAxis, QCPGraph::lsLine, Qt::yellow, "SNR", snrCB, Options::setAnalyzeSNR);

    auto raColor = KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError");
    RA_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, raColor, "RA", raCB, Options::setAnalyzeRA);
    auto decColor = KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError");
    DEC_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, decColor, "DEC", decCB, Options::setAnalyzeDEC);

    QCPAxis *pulseAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    pulseAxis->setVisible(false);
    // 150 is a typical value for pulse-ms/pixel
    // This will roughtly co-incide with the -2,5 range for the ra/dec plots.
    pulseAxis->setRange(-2 * 150, 5 * 150);

    auto raPulseColor = KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError");
    raPulseColor.setAlpha(75);
    RA_PULSE_GRAPH = initGraphAndCB(statsPlot, pulseAxis, QCPGraph::lsLine, raPulseColor, "RAp", raPulseCB,
                                    Options::setAnalyzeRAp);
    statsPlot->graph(RA_PULSE_GRAPH)->setBrush(QBrush(raPulseColor, Qt::Dense4Pattern));

    auto decPulseColor = KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError");
    decPulseColor.setAlpha(75);
    DEC_PULSE_GRAPH = initGraphAndCB(statsPlot, pulseAxis, QCPGraph::lsLine, decPulseColor, "DECp", decPulseCB,
                                     Options::setAnalyzeDECp);
    statsPlot->graph(DEC_PULSE_GRAPH)->setBrush(QBrush(decPulseColor, Qt::Dense4Pattern));

    DRIFT_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, Qt::lightGray, "Drift", driftCB,
                                 Options::setAnalyzeDrift);
    RMS_GRAPH = initGraphAndCB(statsPlot, statsPlot->yAxis, QCPGraph::lsLine, Qt::red, "RMS", rmsCB, Options::setAnalyzeRMS);

    QCPAxis *mountRaDecAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    mountRaDecAxis->setVisible(false);
    mountRaDecAxis->setRange(-10, 370);
    // Colors of these two unimportant--not really plotted.
    MOUNT_RA_GRAPH = initGraphAndCB(statsPlot, mountRaDecAxis, QCPGraph::lsLine, Qt::red, "MOUNT_RA", mountRaCB,
                                    Options::setAnalyzeMountRA);
    MOUNT_DEC_GRAPH = initGraphAndCB(statsPlot, mountRaDecAxis, QCPGraph::lsLine, Qt::red, "MOUNT_DEC", mountDecCB,
                                     Options::setAnalyzeMountDEC);
    MOUNT_HA_GRAPH = initGraphAndCB(statsPlot, mountRaDecAxis, QCPGraph::lsLine, Qt::red, "MOUNT_HA", mountHaCB,
                                    Options::setAnalyzeMountHA);

    QCPAxis *azAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    azAxis->setVisible(false);
    azAxis->setRange(-10, 370);
    AZ_GRAPH = initGraphAndCB(statsPlot, azAxis, QCPGraph::lsLine, Qt::darkGray, "AZ", azCB, Options::setAnalyzeAz);

    QCPAxis *altAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    altAxis->setVisible(false);
    altAxis->setRange(0, 90);
    ALT_GRAPH = initGraphAndCB(statsPlot, altAxis, QCPGraph::lsLine, Qt::white, "ALT", altCB, Options::setAnalyzeAlt);

    QCPAxis *pierSideAxis = statsPlot->axisRect()->addAxis(QCPAxis::atLeft, 0);
    pierSideAxis->setVisible(false);
    pierSideAxis->setRange(-2, 2);
    PIER_SIDE_GRAPH = initGraphAndCB(statsPlot, pierSideAxis, QCPGraph::lsLine, Qt::darkRed, "PierSide", pierSideCB,
                                     Options::setAnalyzePierSide);

    // TODO: Should figure out the margin
    // on the timeline plot, and setting this one accordingly.
    // doesn't look like that's possible with current code, though.
    statsPlot->yAxis->setPadding(50);

    // This makes mouseMove only get called when a button is pressed.
    statsPlot->setMouseTracking(false);

    // Setup the clock-time labels on the x-axis of the stats plot.
    dateTicker.reset(new OffsetDateTimeTicker);
    dateTicker->setDateTimeFormat("hh:mm:ss");
    statsPlot->xAxis->setTicker(dateTicker);

    // Didn't include QCP::iRangeDrag as it  interacts poorly with the curson logic.
    statsPlot->setInteractions(QCP::iRangeZoom);
    statsPlot->axisRect()->setRangeZoomAxes(0, statsPlot->yAxis);
}

// Clear the graphics and state when changing input data.
void Analyze::reset()
{
    maxXValue = 10.0;
    plotStart = 0.0;
    plotWidth = 10.0;

    resetRmsFilter();

    unhighlightTimelineItem();

    for (int i = 0; i < statsPlot->graphCount(); ++i)
        statsPlot->graph(i)->data()->clear();
    statsPlot->clearItems();

    for (int i = 0; i < timelinePlot->graphCount(); ++i)
        timelinePlot->graph(i)->data()->clear();
    timelinePlot->clearItems();

    resetGraphicsPlot();

    infoBox->setText("");
    inputValue->clear();
    captureSessions.clear();
    focusSessions.clear();

    numStarsOut->setText("");
    skyBgOut->setText("");
    snrOut->setText("");
    raOut->setText("");
    decOut->setText("");
    driftOut->setText("");
    rmsOut->setText("");

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

    // Note: no replot().
}

void Analyze::initGraphicsPlot()
{
    initQCP(graphicsPlot);
    FOCUS_GRAPHICS = initGraph(graphicsPlot, graphicsPlot->yAxis,
                               QCPGraph::lsNone, Qt::cyan, "Focus");
    graphicsPlot->graph(FOCUS_GRAPHICS)->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::white, 14));
    FOCUS_GRAPHICS_FINAL = initGraph(graphicsPlot, graphicsPlot->yAxis,
                                     QCPGraph::lsNone, Qt::cyan, "FocusBest");
    graphicsPlot->graph(FOCUS_GRAPHICS_FINAL)->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::yellow, Qt::yellow, 14));
    graphicsPlot->setInteractions(QCP::iRangeZoom);
    graphicsPlot->setInteraction(QCP::iRangeDrag, true);


    GUIDER_GRAPHICS = initGraph(graphicsPlot, graphicsPlot->yAxis,
                                QCPGraph::lsNone, Qt::cyan, "Guide Error");
    graphicsPlot->graph(GUIDER_GRAPHICS)->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssStar, Qt::gray, 5));
}

void Analyze::displayFocusGraphics(const QVector<double> &positions, const QVector<double> &hfrs, bool success)
{
    resetGraphicsPlot();
    auto graph = graphicsPlot->graph(FOCUS_GRAPHICS);
    auto finalGraph = graphicsPlot->graph(FOCUS_GRAPHICS_FINAL);
    double maxHfr = -1e8, maxPosition = -1e8, minHfr = 1e8, minPosition = 1e8;
    for (int i = 0; i < positions.size(); ++i)
    {
        // Yellow circle for the final point.
        if (success && i == positions.size() - 1)
            finalGraph->addData(positions[i], hfrs[i]);
        else
            graph->addData(positions[i], hfrs[i]);
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
        textLabel->setText(QString::number(i + 1));
        textLabel->setFont(QFont(font().family(), 12));
        textLabel->setPen(Qt::NoPen);
        textLabel->setColor(Qt::red);
    }
    const double xRange = maxPosition - minPosition;
    const double yRange = maxHfr - minHfr;
    graphicsPlot->xAxis->setRange(minPosition - xRange * .2, maxPosition + xRange * .2);
    graphicsPlot->yAxis->setRange(minHfr - yRange * .2, maxHfr + yRange * .2);
    graphicsPlot->replot();
}

void Analyze::resetGraphicsPlot()
{
    for (int i = 0; i < graphicsPlot->graphCount(); ++i)
        graphicsPlot->graph(i)->data()->clear();
    graphicsPlot->clearItems();
}

void Analyze::displayFITS(const QString &filename)
{
    QUrl url = QUrl::fromLocalFile(filename);

    if (fitsViewer.isNull())
    {
        if (Options::singleWindowCapturedFITS())
            fitsViewer = KStars::Instance()->genericFITSViewer();
        else
        {
            fitsViewer = new FITSViewer(Options::independentWindowFITS() ? nullptr : KStars::Instance());
            KStars::Instance()->addFITSViewer(fitsViewer);
        }

        fitsViewer->addFITS(url);
        FITSView *currentView = fitsViewer->getCurrentView();
        if (currentView)
            currentView->getImageData()->setAutoRemoveTemporaryFITS(false);
    }
    else
        fitsViewer->updateFITS(url, 0);

    fitsViewer->show();
}

void Analyze::helpMessage()
{
    KHelpClient::invokeHelp(QStringLiteral("tool-ekos.html#ekos-analyze"), QStringLiteral("kstars"));
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
                 .arg(message.size() > 0 ? "," : "")
                 .arg(message));
    appendToLog(line);
}

// Start writing a .analyze file.
void Analyze::startLog()
{
    analyzeStartTime = QDateTime::currentDateTime();
    startTimeInitialized = true;
    if (runtimeDisplay)
        displayStartTime = analyzeStartTime;
    if (logInitialized)
        return;
    QString  dir = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "analyze/";
    if (QDir(dir).exists() == false)
        QDir().mkpath(dir);

    logFilename = dir + "ekos-" + QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss") + ".analyze";
    logFile.setFileName(logFilename);
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);

    // This must happen before the below appendToLog() call.
    logInitialized = true;

    appendToLog(QString("#KStars version %1. Analyze log version 1.0.\n\n")
                .arg(KSTARS_VERSION));
    appendToLog(QString("%1,%2,%3\n")
                .arg("AnalyzeStartTime")
                .arg(analyzeStartTime.toString(timeFormat))
                .arg(analyzeStartTime.timeZoneAbbreviation()));
}

void Analyze::appendToLog(const QString &lines)
{
    if (!logInitialized)
        startLog();
    QTextStream out(&logFile);
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
void Analyze::removeTemporarySession(Session *session)
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
}

// Add a new temporary session.
void Analyze::addTemporarySession(Session *session, double time, double duration,
                                  int y_offset, const QBrush &brush)
{
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
void Analyze::adjustTemporarySession(Session *session)
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
}

// Called when the captureStarting slot receives a signal.
// Saves the message to disk, and calls processCaptureStarting.
void Analyze::captureStarting(double exposureSeconds, const QString &filter)
{
    saveMessage("CaptureStarting",
                QString("%1,%2")
                .arg(QString::number(exposureSeconds, 'f', 3))
                .arg(filter));
    processCaptureStarting(logTime(), exposureSeconds, filter);
}

// Called by either the above (when live data is received), or reading from file.
// BatchMode would be true when reading from file.
void Analyze::processCaptureStarting(double time, double exposureSeconds, const QString &filter, bool batchMode)
{
    captureStartedTime = time;
    captureStartedFilter = filter;
    updateMaxX(time);

    if (!batchMode)
    {
        addTemporarySession(&temporaryCaptureSession, time, 1, CAPTURE_Y, temporaryBrush);
        temporaryCaptureSession.duration = exposureSeconds;
        temporaryCaptureSession.filter = filter;
    }
}

// Called when the captureComplete slot receives a signal.
void Analyze::captureComplete(const QString &filename, double exposureSeconds,
                              const QString &filter, double hfr)
{
    saveMessage("CaptureComplete",
                QString("%1,%2,%3,%4")
                .arg(QString::number(exposureSeconds, 'f', 3))
                .arg(filter)
                .arg(QString::number(hfr, 'f', 3))
                .arg(filename));
    if (runtimeDisplay && captureStartedTime >= 0)
        processCaptureComplete(logTime(), filename, exposureSeconds, filter, hfr);
}

void Analyze::processCaptureComplete(double time, const QString &filename,
                                     double exposureSeconds, const QString &filter,
                                     double hfr, bool batchMode)
{
    removeTemporarySession(&temporaryCaptureSession);
    QBrush stripe;
    if (filterStripeBrush(filter, &stripe))
        addSession(captureStartedTime, time, CAPTURE_Y, successBrush, &stripe);
    else
        addSession(captureStartedTime, time, CAPTURE_Y, successBrush, nullptr);
    captureSessions.add(CaptureSession(captureStartedTime, time, nullptr, false,
                                       filename, exposureSeconds, filter));
    addHFR(hfr, time, captureStartedTime);
    updateMaxX(time);
    if (!batchMode)
        replot();
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
        captureSessions.add(CaptureSession(captureStartedTime, time, nullptr, true, "",
                                           exposureSeconds, captureStartedFilter));
        updateMaxX(time);
        if (!batchMode)
            replot();
        captureStartedTime = -1;
    }
}

void Analyze::resetCaptureState()
{
    captureStartedTime = -1;
    captureStartedFilter = "";
}

void Analyze::autofocusStarting(double temperature, const QString &filter)
{
    saveMessage("AutofocusStarting",
                QString("%1,%2")
                .arg(filter)
                .arg(QString::number(temperature, 'f', 1)));
    processAutofocusStarting(logTime(), temperature, filter);
}

void Analyze::processAutofocusStarting(double time, double temperature, const QString &filter, bool batchMode)
{
    autofocusStartedTime = time;
    autofocusStartedFilter = filter;
    autofocusStartedTemperature = temperature;
    updateMaxX(time);
    if (!batchMode)
    {
        addTemporarySession(&temporaryFocusSession, time, 1, FOCUS_Y, temporaryBrush);
        temporaryFocusSession.temperature = temperature;
        temporaryFocusSession.filter = filter;
    }
}

void Analyze::autofocusComplete(const QString &filter, const QString &points)
{
    saveMessage("AutofocusComplete", QString("%1,%2").arg(filter).arg(points));
    if (runtimeDisplay && autofocusStartedTime >= 0)
        processAutofocusComplete(logTime(), filter, points);
}

void Analyze::processAutofocusComplete(double time, const QString &filter, const QString &points, bool batchMode)
{
    removeTemporarySession(&temporaryFocusSession);
    QBrush stripe;
    if (filterStripeBrush(filter, &stripe))
        addSession(autofocusStartedTime, time, FOCUS_Y, successBrush, &stripe);
    else
        addSession(autofocusStartedTime, time, FOCUS_Y, successBrush, nullptr);
    focusSessions.add(FocusSession(autofocusStartedTime, time, nullptr, true,
                                   autofocusStartedTemperature, filter, points));
    updateMaxX(time);
    if (!batchMode)
        replot();
    autofocusStartedTime = -1;
}

void Analyze::autofocusAborted(const QString &filter, const QString &points)
{
    saveMessage("AutofocusAborted", QString("%1,%2").arg(filter).arg(points));
    if (runtimeDisplay && autofocusStartedTime >= 0)
        processAutofocusAborted(logTime(), filter, points);
}

void Analyze::processAutofocusAborted(double time, const QString &filter, const QString &points, bool batchMode)
{
    removeTemporarySession(&temporaryFocusSession);
    double duration = time - autofocusStartedTime;
    if (autofocusStartedTime >= 0 && duration < 1000)
    {
        // Just in case..
        addSession(autofocusStartedTime, time, FOCUS_Y, failureBrush);
        focusSessions.add(FocusSession(autofocusStartedTime, time, nullptr, false,
                                       autofocusStartedTemperature, filter, points));
        updateMaxX(time);
        if (!batchMode)
            replot();
        autofocusStartedTime = -1;
    }
}

void Analyze::resetAutofocusState()
{
    autofocusStartedTime = -1;
    autofocusStartedFilter = "";
    autofocusStartedTemperature = 0;
}

namespace
{

// TODO: move to ekos.h/cpp?
Ekos::GuideState stringToGuideState(const QString &str)
{
    if (str == I18N_NOOP("Idle"))
        return GUIDE_IDLE;
    else if (str == I18N_NOOP("Aborted"))
        return GUIDE_ABORTED;
    else if (str == I18N_NOOP("Connected"))
        return GUIDE_CONNECTED;
    else if (str == I18N_NOOP("Disconnected"))
        return GUIDE_DISCONNECTED;
    else if (str == I18N_NOOP("Capturing"))
        return GUIDE_CAPTURE;
    else if (str == I18N_NOOP("Looping"))
        return GUIDE_LOOPING;
    else if (str == I18N_NOOP("Subtracting"))
        return GUIDE_DARK;
    else if (str == I18N_NOOP("Subframing"))
        return GUIDE_SUBFRAME;
    else if (str == I18N_NOOP("Selecting star"))
        return GUIDE_STAR_SELECT;
    else if (str == I18N_NOOP("Calibrating"))
        return GUIDE_CALIBRATING;
    else if (str == I18N_NOOP("Calibration error"))
        return GUIDE_CALIBRATION_ERROR;
    else if (str == I18N_NOOP("Calibrated"))
        return GUIDE_CALIBRATION_SUCESS;
    else if (str == I18N_NOOP("Guiding"))
        return GUIDE_GUIDING;
    else if (str == I18N_NOOP("Suspended"))
        return GUIDE_SUSPENDED;
    else if (str == I18N_NOOP("Reacquiring"))
        return GUIDE_REACQUIRE;
    else if (str == I18N_NOOP("Dithering"))
        return GUIDE_DITHERING;
    else if (str == I18N_NOOP("Manual Dithering"))
        return GUIDE_MANUAL_DITHERING;
    else if (str == I18N_NOOP("Dithering error"))
        return GUIDE_DITHERING_ERROR;
    else if (str == I18N_NOOP("Dithering successful"))
        return GUIDE_DITHERING_SUCCESS;
    else if (str == I18N_NOOP("Settling"))
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
        case GUIDE_CALIBRATION_SUCESS:
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
    if (state == G_GUIDING && !batchMode)
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

void Analyze::guideStats(double raError, double decError, int raPulse, int decPulse,
                         double snr, double skyBg, int numStars)
{
    saveMessage("GuideStats", QString("%1,%2,%3,%4,%5,%6,%7")
                .arg(QString::number(raError, 'f', 3))
                .arg(QString::number(decError, 'f', 3))
                .arg(raPulse)
                .arg(decPulse)
                .arg(QString::number(snr, 'f', 3))
                .arg(QString::number(skyBg, 'f', 3))
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
    lastGuideStatsTime = -1;;
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
        if (str == alignStates[i])
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
            return successBrush;
        case ALIGN_FAILED:
            return failureBrush;
        case ALIGN_PROGRESS:
            return progress3Brush;
        case ALIGN_SYNCING:
            return progress2Brush;
        case ALIGN_SLEWING:
            return progressBrush;
        case ALIGN_ABORTED:
            return failureBrush;
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
    if (stateInteresting && !batchMode)
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

const QBrush mountBrush(ISD::Telescope::Status state)
{
    switch (state)
    {
        case ISD::Telescope::MOUNT_IDLE:
            return offBrush;
        case ISD::Telescope::MOUNT_ERROR:
            return failureBrush;
        case ISD::Telescope::MOUNT_MOVING:
        case ISD::Telescope::MOUNT_SLEWING:
            return progressBrush;
        case ISD::Telescope::MOUNT_TRACKING:
            return successBrush;
        case ISD::Telescope::MOUNT_PARKING:
            return stoppedBrush;
        case ISD::Telescope::MOUNT_PARKED:
            return stopped2Brush;
    }
    // Shouldn't get here.
    return offBrush;
}

}  // namespace

// Mount status can be:
// MOUNT_IDLE, MOUNT_MOVING, MOUNT_SLEWING, MOUNT_TRACKING, MOUNT_PARKING, MOUNT_PARKED, MOUNT_ERROR
void Analyze::mountState(ISD::Telescope::Status state)
{
    QString statusString = mountStatusString(state);
    saveMessage("MountState", statusString);
    if (runtimeDisplay)
        processMountState(logTime(), statusString);
}

void Analyze::processMountState(double time, const QString &statusString, bool batchMode)
{
    ISD::Telescope::Status state = toMountStatus(statusString);
    if (mountStateStartedTime >= 0 && lastMountState != ISD::Telescope::MOUNT_IDLE)
    {
        addSession(mountStateStartedTime, time, MOUNT_Y, mountBrush(lastMountState));
        mountSessions.add(MountSession(mountStateStartedTime, time, nullptr, lastMountState));
    }

    if (state != ISD::Telescope::MOUNT_IDLE && !batchMode)
    {
        addTemporarySession(&temporaryMountSession, time, 1, MOUNT_Y,
                            (state == ISD::Telescope::MOUNT_TRACKING) ? successBrush : temporaryBrush);
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
    lastMountState = ISD::Telescope::Status::MOUNT_IDLE;
}

// This message comes from the mount module,
// ra: telescopeCoord.ra().toHMSString()
// dec: telescopeCoord.dec().toDMSString()
// az: telescopeCoord.az().toDMSString()
// alt: telescopeCoord.alt().toDMSString()
// pierSide: currentTelescope->pierSide()
//   which is PIER_UNKNOWN = -1, PIER_WEST = 0, PIER_EAST = 1
void Analyze::mountCoords(const QString &raStr, const QString &decStr,
                          const QString &azStr, const QString &altStr, int pierSide,
                          const QString &haStr)
{
    double ra = dms(raStr, false).Degrees();
    double dec = dms(decStr, true).Degrees();
    double ha = dms(haStr, false).Degrees();
    double az = dms(azStr, true).Degrees();
    double alt = dms(altStr, true).Degrees();

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
                    .arg(QString::number(ra, 'f', 4))
                    .arg(QString::number(dec, 'f', 4))
                    .arg(QString::number(az, 'f', 4))
                    .arg(QString::number(alt, 'f', 4))
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
Mount::MeridianFlipStatus convertMountFlipState(const QString &statusStr)
{
    if (statusStr == "FLIP_NONE")
        return Mount::FLIP_NONE;
    else if (statusStr == "FLIP_PLANNED")
        return Mount::FLIP_PLANNED;
    else if (statusStr == "FLIP_WAITING")
        return Mount::FLIP_WAITING;
    else if (statusStr == "FLIP_ACCEPTED")
        return Mount::FLIP_ACCEPTED;
    else if (statusStr == "FLIP_RUNNING")
        return Mount::FLIP_RUNNING;
    else if (statusStr == "FLIP_COMPLETED")
        return Mount::FLIP_COMPLETED;
    else if (statusStr == "FLIP_ERROR")
        return Mount::FLIP_ERROR;
    return Mount::FLIP_ERROR;
}

QBrush mountFlipStateBrush(Mount::MeridianFlipStatus state)
{
    switch (state)
    {
        case Mount::FLIP_NONE:
            return offBrush;
        case Mount::FLIP_PLANNED:
            return stoppedBrush;
        case Mount::FLIP_WAITING:
            return stopped2Brush;
        case Mount::FLIP_ACCEPTED:
            return progressBrush;
        case Mount::FLIP_RUNNING:
            return progress2Brush;
        case Mount::FLIP_COMPLETED:
            return successBrush;
        case Mount::FLIP_ERROR:
            return failureBrush;
    }
    // Shouldn't get here.
    return offBrush;
}
}  // namespace

void Analyze::mountFlipStatus(Mount::MeridianFlipStatus state)
{
    if (state == lastMountFlipStateReceived)
        return;
    lastMountFlipStateReceived = state;

    QString stateStr = Mount::meridianFlipStatusString(state);
    saveMessage("MeridianFlipState", stateStr);
    if (runtimeDisplay)
        processMountFlipState(logTime(), stateStr);

}

// FLIP_NONE FLIP_PLANNED FLIP_WAITING FLIP_ACCEPTED FLIP_RUNNING FLIP_COMPLETED FLIP_ERROR
void Analyze::processMountFlipState(double time, const QString &statusString, bool batchMode)
{
    Mount::MeridianFlipStatus state = convertMountFlipState(statusString);
    if (state == lastMountFlipStateStarted)
        return;

    bool lastStateInteresting =
        (lastMountFlipStateStarted == Mount::FLIP_PLANNED ||
         lastMountFlipStateStarted == Mount::FLIP_WAITING ||
         lastMountFlipStateStarted == Mount::FLIP_ACCEPTED ||
         lastMountFlipStateStarted == Mount::FLIP_RUNNING);
    if (mountFlipStateStartedTime >= 0 && lastStateInteresting)
    {
        if (state == Mount::FLIP_COMPLETED || state == Mount::FLIP_ERROR)
        {
            // These states are really commentaries on the previous states.
            addSession(mountFlipStateStartedTime, time, MERIDIAN_FLIP_Y, mountFlipStateBrush(state));
            mountFlipSessions.add(MountFlipSession(mountFlipStateStartedTime, time, nullptr, state));
        }
        else
        {
            addSession(mountFlipStateStartedTime, time, MERIDIAN_FLIP_Y, mountFlipStateBrush(lastMountFlipStateStarted));
            mountFlipSessions.add(MountFlipSession(mountFlipStateStartedTime, time, nullptr, lastMountFlipStateStarted));
        }
    }
    bool stateInteresting =
        (state == Mount::FLIP_PLANNED ||
         state == Mount::FLIP_WAITING ||
         state == Mount::FLIP_ACCEPTED ||
         state == Mount::FLIP_RUNNING);
    if (stateInteresting && !batchMode)
    {
        addTemporarySession(&temporaryMountFlipSession, time, 1, MERIDIAN_FLIP_Y, temporaryBrush);
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
    lastMountFlipStateReceived = Mount::FLIP_NONE;
    lastMountFlipStateStarted = Mount::FLIP_NONE;
    mountFlipStateStartedTime = -1;
}
}
