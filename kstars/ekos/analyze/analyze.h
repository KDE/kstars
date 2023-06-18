/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ANALYZE_H
#define ANALYZE_H

#include <memory>
#include "qcustomplot.h"
#include "ekos/ekos.h"
#include "ekos/mount/mount.h"
#include "indi/indimount.h"
#include "yaxistool.h"
#include "ui_analyze.h"

class FITSViewer;
class OffsetDateTimeTicker;

namespace Ekos
{

class RmsFilter;

/**
 *@class Analyze
 *@short Analysis tab for Ekos sessions.
 *@author Hy Murveit
 *@version 1.0
 */
class Analyze : public QWidget, public Ui::Analyze
{
        Q_OBJECT

    public:
        Analyze();
        ~Analyze();

        // Baseclass used to represent a segment of Timeline data.
        class Session
        {
            public:
                // Start and end time in seconds since start of the log.
                double start, end;
                // y-offset for the timeline plot. Each line uses a different integer.
                int offset;
                // Variables used in temporary sessions. A temporary session
                // represents a process that has started but not yet finished.
                // Those are plotted "for now", but will be replaced by the
                // finished line when the process completes.
                // Rect is the temporary graphic on the Timeline, and
                // temporaryBrush defines its look.
                QCPItemRect *rect;
                QBrush temporaryBrush;

                Session(double s, double e, int o, QCPItemRect *r)
                    : start(s), end(e), offset(o), rect(r) {}

                // These 2 are used to build tables for the details display.
                void setupTable(const QString &name, const QString &status,
                                const QDateTime &startClock, const QDateTime &endClock,
                                QTableWidget *table);
                void addRow(const QString &key, const QString &value);

                // True if this session is temporary.
                bool isTemporary() const;

            private:
                QTableWidget *details;
                QString htmlString;
        };
        // Below are subclasses of Session used to represent all the different
        // lines in the Timeline. Each of those hold different types of information
        // about the process it represents.
        class CaptureSession : public Session
        {
            public:
                bool aborted;
                QString filename;
                double duration;
                QString filter;
                double hfr;
                CaptureSession(double start_, double end_, QCPItemRect *rect,
                               bool aborted_, const QString &filename_,
                               double duration_, const QString &filter_)
                    : Session(start_, end_, CAPTURE_Y, rect),
                      aborted(aborted_), filename(filename_),
                      duration(duration_), filter(filter_), hfr(0) {}
                CaptureSession() : Session(0, 0, CAPTURE_Y, nullptr) {}
        };
        // Guide sessions collapse some of the possible guiding states.
        // SimpleGuideState are those collapsed states.
        typedef enum
        {
            G_IDLE, G_GUIDING, G_CALIBRATING, G_SUSPENDED, G_DITHERING, G_IGNORE
        } SimpleGuideState;
        class GuideSession : public Session
        {
            public:
                SimpleGuideState simpleState;
                GuideSession(double start_, double end_, QCPItemRect *rect, SimpleGuideState state_)
                    : Session(start_, end_, GUIDE_Y, rect), simpleState(state_) {}
                GuideSession() : Session(0, 0, GUIDE_Y, nullptr) {}
        };
        class AlignSession : public Session
        {
            public:
                AlignState state;
                AlignSession(double start_, double end_, QCPItemRect *rect, AlignState state_)
                    : Session(start_, end_, ALIGN_Y, rect), state(state_) {}
                AlignSession() : Session(0, 0, ALIGN_Y, nullptr) {}
        };
        class MountSession : public Session
        {
            public:
                ISD::Mount::Status state;
                MountSession(double start_, double end_, QCPItemRect *rect, ISD::Mount::Status state_)
                    : Session(start_, end_, MOUNT_Y, rect), state(state_) {}
                MountSession() : Session(0, 0, MOUNT_Y, nullptr) {}
        };
        class MountFlipSession : public Session
        {
            public:
                MeridianFlipState::MeridianFlipMountState state;
                MountFlipSession(double start_, double end_, QCPItemRect *rect, MeridianFlipState::MeridianFlipMountState state_)
                    : Session(start_, end_, MERIDIAN_MOUNT_FLIP_Y, rect), state(state_) {}
                MountFlipSession() : Session(0, 0, MERIDIAN_MOUNT_FLIP_Y, nullptr) {}
        };
        class SchedulerJobSession : public Session
        {
            public:
                SchedulerJobSession(double start_, double end_, QCPItemRect *rect,
                                    const QString &jobName_, const QString &reason_)
                    : Session(start_, end_, SCHEDULER_Y, rect), jobName(jobName_), reason(reason_) {}
                SchedulerJobSession() : Session(0, 0, SCHEDULER_Y, nullptr) {}
                QString jobName;
                QString reason;
        };
        class FocusSession : public Session
        {
            public:
                bool success;
                double temperature;
                QString filter;

                // Standard focus parameters
                QString points;
                QString curve;
                QString title;
                QVector<double> positions; // Double to be more friendly to QCustomPlot addData.
                QVector<double> hfrs;

                // Adaptive focus parameters
                int tempTicks;
                double altitude;
                int altTicks, totalTicks, adaptedPosition;

                // false for adaptiveFocus.
                bool standardSession = true;

                FocusSession() : Session(0, 0, FOCUS_Y, nullptr) {}
                FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                             const QString &filter_, const QString &points_, const QString &curve_, const QString &title_);
                FocusSession(double start_, double end_, QCPItemRect *rect,
                             const QString &filter_, double temperature_, int tempTicks_, double altitude_,
                             int altTicks_, int totalTicks_, int position_);
                double focusPosition();
        };

    public slots:
        // These slots are messages received from the different Ekos processes
        // used to gather data about those processes.

        // From Capture
        void captureComplete(const QVariantMap &metadata);
        void captureStarting(double exposureSeconds, const QString &filter);
        void captureAborted(double exposureSeconds);

        // From Guide
        void guideState(Ekos::GuideState status);
        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

        // From Focus
        void autofocusStarting(double temperature, const QString &filter);
        void autofocusComplete(const QString &filter, const QString &points, const QString &curve, const QString &title);
        void adaptiveFocusComplete(const QString &filter, double temperature, int tempTicks,
                                   double altitude, int altTicks, int totalTicks, int position);
        void autofocusAborted(const QString &filter, const QString &points);
        void newTemperature(double temperatureDelta, double temperature);

        // From Align
        void alignState(Ekos::AlignState state);

        // From Mount
        void mountState(ISD::Mount::Status status);
        void mountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &haValue);
        void mountFlipStatus(Ekos::MeridianFlipState::MeridianFlipMountState status);

        void schedulerJobStarted(const QString &jobName);
        void schedulerJobEnded(const QString &jobName, const QString &endReason);
        void newTargetDistance(double targetDistance);

        // From YAxisTool
        void userChangedYAxis(QObject *key, const YAxisInfo &axisInfo);
        void userSetLeftAxis(QCPAxis *axis);
        void userSetAxisColor(QObject *key, const YAxisInfo &axisInfo, const QColor &color);

        void yAxisRangeChanged(const QCPRange &newRange);

    private slots:

    signals:

    private:

        // The file-reading, processInputLine(), and signal-slot codepaths share the methods below
        // to process their messages. Time is the offset in seconds from the start of the log.
        // BatchMode is true in the file reading path. It means don't call replot() as there may be
        // many more messages to come. The rest of the args are specific to the message type.
        void processCaptureStarting(double time, double exposureSeconds, const QString &filter, bool batchMode = false);
        void processCaptureComplete(double time, const QString &filename, double exposureSeconds, const QString &filter,
                                    double hfr, int numStars, int median, double eccentricity, bool batchMode = false);
        void processCaptureAborted(double time, double exposureSeconds, bool batchMode = false);
        void processAutofocusStarting(double time, double temperature, const QString &filter, bool batchMode = false);
        void processAutofocusComplete(double time, const QString &filter, const QString &points, const QString &curve,
                                      const QString &title, bool batchMode = false);
        void processAdaptiveFocusComplete(double time, const QString &filter, double temperature, int tempTicks,
                                          double altitude, int altTicks, int totalTicks, int position, bool batchMode = false);
        void processAutofocusAborted(double time, const QString &filter, const QString &points, bool batchMode = false);
        void processTemperature(double time, double temperature, bool batchMode = false);
        void processGuideState(double time, const QString &state, bool batchMode = false);
        void processGuideStats(double time, double raError, double decError, int raPulse,
                               int decPulse, double snr, double skyBg, int numStars, bool batchMode = false);
        void processMountCoords(double time, double ra, double dec, double az, double alt,
                                int pierSide, double ha, bool batchMode = false);

        void processMountState(double time, const QString &statusString, bool batchMode = false);
        void processAlignState(double time, const QString &statusString, bool batchMode = false);
        void processMountFlipState(double time, const QString &statusString, bool batchMode = false);

        void processSchedulerJobStarted(double time, const QString &jobName, bool batchMode = false);
        void processSchedulerJobEnded(double time, const QString &jobName, const QString &reason, bool batchMode = false);
        void checkForMissingSchedulerJobEnd(double time);
        void processTargetDistance(double time, double targetDistance, bool batchMode = false);

        // Plotting primatives.
        void replot(bool adjustSlider = true);
        void zoomIn();
        void zoomOut();
        void scroll(int value);
        void scrollRight();
        void scrollLeft();
        void statsYZoom(double zoomAmount);
        void statsYZoomIn();
        void statsYZoomOut();


        // maxXValue keeps the largest time offset we've received so far.
        // It represents the extent of the plots (0 -> maxXValue).
        // This is called each time a message is received in case that message's
        // time is past the current value of maxXValue.
        void updateMaxX(double time);

        // Callbacks for when the timeline is clicked. ProcessTimelineClick
        // will determine which segment on which line was clicked and then
        // call captureSessionClicked() or focusSessionClicked, etc.
        void processTimelineClick(QMouseEvent *event, bool doubleClick);
        void captureSessionClicked(CaptureSession &c, bool doubleClick);
        void focusSessionClicked(FocusSession &c, bool doubleClick);
        void guideSessionClicked(GuideSession &c, bool doubleClick);
        void mountSessionClicked(MountSession &c, bool doubleClick);
        void alignSessionClicked(AlignSession &c, bool doubleClick);
        void mountFlipSessionClicked(MountFlipSession &c, bool doubleClick);
        void schedulerSessionClicked(SchedulerJobSession &c, bool doubleClick);

        // Low-level callbacks.
        // These two call processTimelineClick().
        void timelineMousePress(QMouseEvent *event);
        void timelineMouseDoubleClick(QMouseEvent *event);
        // Calls zoomIn or zoomOut.
        void timelineMouseWheel(QWheelEvent *event);
        // Sets the various displays visbile or not according to the checkboxes.
        void setVisibility();

        void processStatsClick(QMouseEvent *event, bool doubleClick);
        void statsMousePress(QMouseEvent *event);
        void statsMouseDoubleClick(QMouseEvent *event);
        void statsMouseMove(QMouseEvent *event);
        void setupKeyboardShortcuts(QWidget *plot);

        // (Un)highlights a segment on the timeline after one is clicked.
        // This indicates which segment's data is displayed in the
        // graphicsPlot and details table.
        void highlightTimelineItem(double y, double start, double end);
        void unhighlightTimelineItem();

        // logTime() returns the number of seconds between "now" or "time" and
        // the start of the log. They are useful for recording signal and storing
        // them to file. They are not useful when reading data from files.
        double logTime();
        // Returns the number of seconds between time and the start of the log.
        double logTime(const QDateTime &time);
        // Goes back from logSeconds to human-readable clock time.
        QDateTime clockTime(double logSeconds);

        // Add a new segment to the Timeline graph.
        // Returns a rect item, which is only important temporary objects, who
        // need to erase the item when the temporary session is removed.
        // This memory is owned by QCustomPlot and shouldn't be freed.
        // This pointer is stored in Session::rect.
        QCPItemRect * addSession(double start, double end, double y,
                                 const QBrush &brush, const QBrush *stripeBrush = nullptr);

        // Manage temporary sessions (only used for live data--file-reading doesn't
        // need temporary sessions). For example, when an image capture has started
        // but not yet completed, a temporary session is added to the timeline to
        // represent the not-yet-completed capture.
        void addTemporarySession(Session *session, double time, double duration,
                                 int y_offset, const QBrush &brush);
        void removeTemporarySession(Session *session);
        void removeTemporarySessions();
        void adjustTemporarySession(Session *session);
        void adjustTemporarySessions();

        // Add new stats to the statsPlot.
        void addGuideStats(double raDrift, double decDrift, int raPulse, int decPulse,
                           double snr, int numStars, double skyBackground, double time);
        void addGuideStatsInternal(double raDrift, double decDrift, double raPulse,
                                   double decPulse, double snr, double numStars,
                                   double skyBackground, double drift, double rms, double time);
        void addMountCoords(double ra, double dec, double az, double alt, int pierSide,
                            double ha, double time);
        void addHFR(double hfr, int numCaptureStars, int median, double eccentricity,
                    const double time, double startTime);
        void addTemperature(double temperature, const double time);
        void addFocusPosition(double focusPosition, double time);
        void addTargetDistance(double targetDistance, const double time);

        // Initialize the graphs (axes, linestyle, pen, name, checkbox callbacks).
        // Returns the graph index.
        int initGraph(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                      const QColor &color, const QString &name);
        template <typename Func>
        int initGraphAndCB(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                           const QColor &color, const QString &name, const QString &shortName,
                           QCheckBox *cb, Func setCb, QLineEdit *out = nullptr);

        // Make graphs visible/invisible & add/delete them from the legend.
        void toggleGraph(int graph_id, bool show);

        // Initializes the main QCustomPlot windows.
        void initStatsPlot();
        void initTimelinePlot();
        void initGraphicsPlot();
        void initInputSelection();

        // Displays the focus positions and HFRs on the graphics plot.
        void displayFocusGraphics(const QVector<double> &positions, const QVector<double> &hfrs, const QString &curve, const QString &title, bool success);
        // Displays the guider ra and dec drift plot, and computes RMS errors.
        void displayGuideGraphics(double start, double end, double *raRMS,
                                  double *decRMS, double *totalRMS, int *numSamples);

        // Updates the stats value display boxes next to their checkboxes.
        void updateStatsValues();
        // Manages the statsPlot cursor.
        void setStatsCursor(double time);
        void removeStatsCursor();
        void keepCurrent(int state);

        // Restore checkboxs from Options.
        void initStatsCheckboxes();

        // Clears the data, resets the various plots & displays.
        void reset();
        void resetGraphicsPlot();

        // Resets the variables used to process the signals received.
        void resetCaptureState();
        void resetAutofocusState();
        void resetGuideState();
        void resetGuideStats();
        void resetAlignState();
        void resetMountState();
        void resetMountCoords();
        void resetMountFlipState();
        void resetSchedulerJob();
        void resetTemperature();

        // Read and display an input .analyze file.
        double readDataFromFile(const QString &filename);
        double processInputLine(const QString &line);

        // Opens a FITS file for viewing.
        void displayFITS(const QString &filename);

        // Pop up a help-message window.
        void helpMessage();

        // Write the analyze log file message.
        void saveMessage(const QString &type, const QString &message);
        // low level file writing.
        void startLog();
        void appendToLog(const QString &lines);

        // Used to capture double clicks on stats output QLineEdits to set y-axis limits.
        bool eventFilter(QObject *o, QEvent *e) override;
        QTimer clickTimer;
        YAxisInfo m_ClickTimerInfo;

        // Utility that adds a y-axis to the stats plot.
        QCPAxis *newStatsYAxis(const QString &label, double lower = YAxisInfo::LOWER_RESCALE,
                               double upper = YAxisInfo::UPPER_RESCALE);

        // Save and restore user-updated y-axis limits.
        QString serializeYAxes();
        bool restoreYAxes(const QString &encoding);

        // Sets the y-axis to be displayed on the left of the statsPlot.
        void setLeftAxis(QCPAxis *axis);
        void updateYAxisMap(QObject *key, const YAxisInfo &axisInfo);

        // The pop-up allowing users to edit y-axis lower and upper graph values.
        YAxisTool m_YAxisTool;

        // The y-axis values displayed to the left of the stat's graph.
        QCPAxis *activeYAxis { nullptr };

        void startYAxisTool(QObject *key, const YAxisInfo &info);

        // Map connecting QLineEdits to Y-Axes, so when a QLineEdit is double clicked,
        // the corresponding y-axis can be found.
        std::map<QObject*, YAxisInfo> yAxisMap;

        // The .analyze log file being written.
        QString logFilename { "" };
        QFile logFile;
        bool logInitialized { false };

        // These define the view for the timeline and stats plots.
        // The plots start plotStart seconds from the start of the session, and
        // are plotWidth seconds long. The end of the X-axis is maxXValue.
        double plotStart { 0.0 };
        double plotWidth { 10.0 };
        double maxXValue { 10.0 };

        // Data are displayed in seconds since the session started.
        // analyzeStartTime is when the session started, used to translate to clock time.
        QDateTime analyzeStartTime;
        QString analyzeTimeZone { "" };
        bool startTimeInitialized { false };

        // displayStartTime is similar to analyzeStartTime, but references the
        // start of the log being displayed (e.g. if reading from a file).
        // When displaying the current session it should equal analyzeStartTime.
        QDateTime displayStartTime;

        // AddGuideStats uses RmsFilter to compute RMS values of the squared
        // RA and DEC errors, thus calculating the RMS error.
        std::unique_ptr<RmsFilter> guiderRms;
        std::unique_ptr<RmsFilter> captureRms;

        // Used to keep track of the y-axis position when moving it with the mouse.
        double yAxisInitialPos = { 0 };

        // Used to display clock-time on the X-axis.
        QSharedPointer<OffsetDateTimeTicker> dateTicker;

        // The rectangle over the current selection.
        // Memory owned by QCustomPlot.
        QCPItemRect *selectionHighlight { nullptr };

        // FITS Viewer to display FITS images.
        QPointer<FITSViewer> fitsViewer;
        // When trying to load a FITS file, if the original file path doesn't
        // work, Analyze tries to find the file under the alternate folder.
        QString alternateFolder;

        // The vertical line in the stats plot.
        QCPItemLine *statsCursor { nullptr };
        QCPItemLine *timelineCursor { nullptr };
        double statsCursorTime { -1 };

        // Keeps the directory from the last time the user loaded a .analyze file.
        QUrl dirPath;

        // True if Analyze is displaying data as it comes in from the other modules.
        // False if Analyze is displaying data read from a file.
        bool runtimeDisplay { true };

        // When a module's session is ongoing, we represent it as a "temporary session"
        // which will be replaced once the session is done.
        CaptureSession temporaryCaptureSession;
        FocusSession temporaryFocusSession;
        GuideSession temporaryGuideSession;
        AlignSession temporaryAlignSession;
        MountSession temporaryMountSession;
        MountFlipSession temporaryMountFlipSession;
        SchedulerJobSession temporarySchedulerJobSession;

        // Capture state-machine variables.
        double captureStartedTime { -1 };
        double previousCaptureStartedTime { 1 };
        double previousCaptureCompletedTime { 1 };
        QString captureStartedFilter { "" };

        // Autofocus state-machine variables.
        double autofocusStartedTime { -1 };
        QString autofocusStartedFilter { "" };
        double autofocusStartedTemperature { 0 };

        // GuideState state-machine variables.
        SimpleGuideState lastGuideStateStarted { G_IDLE };
        double guideStateStartedTime { -1 };

        // GuideStats state-machine variables.
        double lastGuideStatsTime { -1 };
        double lastCaptureRmsTime { -1 };
        int numStarsMax { 0 };
        double snrMax { 0 };
        double skyBgMax { 0 };
        int medianMax { 0 };
        int numCaptureStarsMax { 0 };
        double lastTemperature { -1000 };

        // AlignState state-machine variables.
        AlignState lastAlignStateReceived { ALIGN_IDLE };
        AlignState lastAlignStateStarted { ALIGN_IDLE };
        double lastAlignStateStartedTime { -1 };

        // MountState state-machine variables.
        double mountStateStartedTime { -1 };
        ISD::Mount::Status lastMountState { ISD::Mount::Status::MOUNT_IDLE };

        // Mount coords state machine variables.
        // Used to filter out mount Coords messages--we only process ones
        // where the values have changed significantly.
        double lastMountRa { -1 };
        double lastMountDec { -1 };
        double lastMountHa { -1 };
        double lastMountAz { -1 };
        double lastMountAlt { -1 };
        int lastMountPierSide { -1 };

        // Flip state machine variables
        MeridianFlipState::MeridianFlipMountState lastMountFlipStateReceived { MeridianFlipState::MOUNT_FLIP_NONE};
        MeridianFlipState::MeridianFlipMountState lastMountFlipStateStarted { MeridianFlipState::MOUNT_FLIP_NONE };
        double mountFlipStateStartedTime { -1 };

        // SchedulerJob state machine variables
        double schedulerJobStartedTime;
        QString schedulerJobStartedJobName;

        QMap<QString, QColor> schedulerJobColors;
        QBrush schedulerJobBrush(const QString &jobName, bool temporary);

        // Y-offsets for the timeline plot for the various modules.
        static constexpr int CAPTURE_Y = 1;
        static constexpr int FOCUS_Y = 2;
        static constexpr int ALIGN_Y = 3;
        static constexpr int GUIDE_Y = 4;
        static constexpr int MERIDIAN_MOUNT_FLIP_Y = 5;
        static constexpr int MOUNT_Y = 6;
        static constexpr int SCHEDULER_Y = 7;
        static constexpr int LAST_Y = 8;
};
}


#endif // Analyze
