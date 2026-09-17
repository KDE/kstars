/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ANALYZE_H
#define ANALYZE_H

#include <memory>
#include "ekos/ekos.h"
#include "ekos/mount/mount.h"
#include "indi/indimount.h"
#include "yaxistool.h"
#include "ui_analyze.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/focus/focusutils.h"
#include "Options.h"

class FITSViewer;
class OffsetDateTimeTicker;
class QCustomPlot;
class QMenu;

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

                // Device name (camera or focuser) this session belongs to.
                // Empty for sessions of subsystems that aren't per-device
                // (Guide, Align, Mount, Flip, Scheduler).
                QString device;

                Session(double s, double e, int o, QCPItemRect *r)
                    : start(s), end(e), offset(o), rect(r) {}

                Session() : start(0), end(0), offset(0), rect(nullptr) {}

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
                // Row placement depends on which device this session belongs to
                // (see Analyze::captureRow()), so offset is set by the caller
                // after construction rather than baked in here.
                CaptureSession(double start_, double end_, QCPItemRect *rect,
                               bool aborted_, const QString &filename_,
                               double duration_, const QString &filter_, const QString &device_ = "")
                    : Session(start_, end_, 0, rect),
                      aborted(aborted_), filename(filename_),
                      duration(duration_), filter(filter_), hfr(0)
                {
                    device = device_;
                }
                CaptureSession() : Session(0, 0, 0, nullptr) {}
        };
        // Guide sessions collapse some of the possible guiding states.
        // SimpleGuideState are those collapsed states.
        typedef enum
        {
            G_IDLE, G_GUIDING, G_CALIBRATING, G_SUSPENDED, G_DITHERING, G_IGNORE
        } SimpleGuideState;
        // Guide/Align/Mount/Flip/Scheduler each occupy a single row, but that
        // row's Y-value is a runtime value (it shifts depending on how many
        // Capture/Focus device rows precede it), not a compile-time constant,
        // so the caller sets offset explicitly after construction, same as
        // CaptureSession/FocusSession.
        class GuideSession : public Session
        {
            public:
                SimpleGuideState simpleState;
                GuideSession(double start_, double end_, QCPItemRect *rect, SimpleGuideState state_)
                    : Session(start_, end_, 0, rect), simpleState(state_) {}
                GuideSession() : Session(0, 0, 0, nullptr) {}
        };
        class AlignSession : public Session
        {
            public:
                AlignState state;
                AlignSession(double start_, double end_, QCPItemRect *rect, AlignState state_)
                    : Session(start_, end_, 0, rect), state(state_) {}
                AlignSession() : Session(0, 0, 0, nullptr) {}
        };
        class MountSession : public Session
        {
            public:
                ISD::Mount::Status state;
                MountSession(double start_, double end_, QCPItemRect *rect, ISD::Mount::Status state_)
                    : Session(start_, end_, 0, rect), state(state_) {}
                MountSession() : Session(0, 0, 0, nullptr) {}
        };
        class MountFlipSession : public Session
        {
            public:
                MeridianFlipState::MeridianFlipMountState state;
                MountFlipSession(double start_, double end_, QCPItemRect *rect, MeridianFlipState::MeridianFlipMountState state_)
                    : Session(start_, end_, 0, rect), state(state_) {}
                MountFlipSession() : Session(0, 0, 0, nullptr) {}
        };
        class SchedulerJobSession : public Session
        {
            public:
                SchedulerJobSession(double start_, double end_, QCPItemRect *rect,
                                    const QString &jobName_, const QString &reason_)
                    : Session(start_, end_, 0, rect), jobName(jobName_), reason(reason_) {}
                SchedulerJobSession() : Session(0, 0, 0, nullptr) {}
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
                AutofocusReason reason;
                QString reasonInfo;
                QString points;
                bool useWeights;
                QString curve;
                QString title;
                AutofocusFailReason failCode;
                QString failCodeInfo;
                QVector<double> positions; // Double to be more friendly to QCustomPlot addData.
                QVector<double> hfrs;
                QVector<double> weights;
                QVector<bool> outliers;

                // Adaptive focus parameters
                double tempTicks, altitude, altTicks;
                int prevPosError, thisPosError, totalTicks, adaptedPosition;

                // false for adaptiveFocus.
                bool standardSession = true;

                // Row placement depends on which device this session belongs to
                // (see Analyze::focusRow()), so offset is set by the caller
                // after construction rather than baked in here.
                FocusSession() : Session(0, 0, 0, nullptr) {}
                FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                             const QString &filter_, const QString &points_, const QString &curve_, const QString &title_,
                             const QString &device_ = "");
                FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                             const QString &filter_, const AutofocusReason reason_, const QString &reasonInfo_, const QString &points_,
                             const bool useWeights_,
                             const QString &curve_, const QString &title_, const AutofocusFailReason failCode_, const QString failCodeInfo_,
                             const QString &device_ = "");
                FocusSession(double start_, double end_, QCPItemRect *rect,
                             const QString &filter_, double temperature_, double tempTicks_, double altitude_,
                             double altTicks_, int prevPosError, int thisPosError, int totalTicks_, int position_,
                             const QString &device_ = "");
                double focusPosition();
        };

        // This will cause the .analyze file to reset -- that is, stop appending to any
        // .analyze file that's already open and start writing a new .analyze file.
        // All graphics will be reset such that the session begins "now".
        void restart();

        void clearLog();
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }

    public Q_SLOTS:
        // These slots are messages received from the different Ekos processes
        // used to gather data about those processes.

        // From Capture
        void captureComplete(const QVariantMap &metadata, const QString &trainname);
        void captureStarting(double exposureSeconds, const QString &filter, const QString &trainname);
        void captureAborted(double exposureSeconds, const QString &trainname);
        // A camera tab was created or reassigned to this device (see
        // Capture::cameraDeviceActive()). Adds it to the Timeline's live
        // camera rows for this session; devices are never removed on this
        // path (not even when their tab closes -- see the signal's doc),
        // so earlier events stay visible.
        void addActiveCamera(const QString &device);

        // From Guide
        void guideState(Ekos::GuideState status);
        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

        // From Focus
        void autofocusStarting(double temperature, const QString &filter, const AutofocusReason reason, const QString &reasonInfo,
                               const QString &trainname);
        void autofocusComplete(const double temperature, const QString &filter, const QString &points, const bool useWeights,
                               const QString &curve, const QString &title, const QString &trainname);
        void adaptiveFocusComplete(const QString &filter, double temperature, double tempTicks,
                                   double altitude, double altTicks, int prevPosError, int thisPosError, int totalTicks,
                                   int position, bool focuserMoved, const QString &trainname);
        void autofocusAborted(const QString &filter, const QString &points, const bool useWeights,
                              const AutofocusFailReason failCode, const QString failCodeInfo, const QString &trainname);
        void newTemperature(double temperatureDelta, double temperature, const QString &trainname);
        // As addActiveCamera() above, but for focusers (see
        // FocusModule::focuserDeviceActive()).
        void addActiveFocuser(const QString &device);

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

        void appendLogText(const QString &);

    private Q_SLOTS:

    Q_SIGNALS:
        void newLog(const QString &text);

    private:

        // The file-reading, processInputLine(), and signal-slot codepaths share the methods below
        // to process their messages. Time is the offset in seconds from the start of the log.
        // BatchMode is true in the file reading path. It means don't call replot() as there may be
        // many more messages to come. The rest of the args are specific to the message type.
        void processCaptureStarting(double time, double exposureSeconds, const QString &filter, const QString &device = "");
        void processCaptureComplete(double time, const QString &filename, double exposureSeconds, const QString &filter,
                                    double hfr, int numStars, int median, double eccentricity, bool batchMode = false,
                                    const QString &device = "");
        void processCaptureAborted(double time, double exposureSeconds, bool batchMode = false, const QString &device = "");
        void processAutofocusStarting(double time, double temperature, const QString &filter, const AutofocusReason reason,
                                      const QString &reasonInfo, const QString &device = "");
        void processAutofocusComplete(double time, const QString &filter, const QString &points, const QString &curve,
                                      const QString &title, bool batchMode);
        void processAutofocusCompleteV2(double time, double temperature, const QString &filter, const AutofocusReason reason,
                                        const QString &reasonInfo,
                                        const QString &points, const bool useWeights, const QString &curve, const QString &title,
                                        bool batchMode = false, const QString &device = "");
        void processAutofocusAborted(double time, const QString &filter, const QString &points, bool batchMode);
        void processAutofocusAbortedV2(double time, double temperature, const QString &filter, const AutofocusReason reason,
                                       const QString &reasonInfo,
                                       const QString &points, const bool useWeights, const AutofocusFailReason failCode, const QString failCodeInfo,
                                       bool batchMode = false, const QString &device = "");
        void processAdaptiveFocusComplete(double time, const QString &filter, double temperature, double tempTicks,
                                          double altitude, double altTicks, int prevPosError, int thisPosError, int totalTicks,
                                          int position, bool focuserMoved, bool batchMode = false, const QString &device = "");
        void processTemperature(double time, double temperature, bool batchMode = false, const QString &device = "");
        void processGuideState(double time, const QString &state, bool batchMode = false);
        void processGuideStats(double time, double raError, double decError, int raPulse,
                               int decPulse, double snr, double skyBg, int numStars, bool batchMode = false);
        void processMountCoords(double time, double ra, double dec, double az, double alt,
                                int pierSide, double ha, bool batchMode = false);

        void processMountState(double time, const QString &statusString, bool batchMode = false);
        void processAlignState(double time, const QString &statusString, bool batchMode = false);
        void processMountFlipState(double time, const QString &statusString, bool batchMode = false);

        void processSchedulerJobStarted(double time, const QString &jobName);
        void processSchedulerJobEnded(double time, const QString &jobName, const QString &reason, bool batchMode = false);
        void checkForMissingSchedulerJobEnd(double time);
        void processTargetDistance(double time, double targetDistance, bool batchMode = false);

        // Plotting primitives.
        void replot(bool adjustSlider = true);
        void zoomIn();
        void zoomOut();
        void scroll(int value);
        void scrollRight();
        void scrollLeft();
        void statsYZoom(double zoomAmount);
        void statsYZoomIn();
        void statsYZoomOut();
        // Return true if the session is visible on the plots.
        bool isVisible(const Session &s) const;
        // Shift the view so that time is at the center (keeping the current plot width).
        void adjustView(double time);


        // maxXValue keeps the largest time offset we've received so far.
        // It represents the extent of the plots (0 -> maxXValue).
        // This is called each time a message is received in case that message's
        // time is past the current value of maxXValue.
        void updateMaxX(double time);

        // Callbacks for when the timeline is clicked. ProcessTimelineClick
        // will determine which segment on which line was clicked and then
        // call captureSessionClicked() or focusSessionClicked, etc.
        void processTimelineClick(QMouseEvent *event, bool doubleClick);
        // updateDeviceSelection is false when this is called from the "keep current"
        // auto-follow path on live completion events, so the device combo selection
        // (a user choice) isn't yanked around by unrelated devices' activity.
        void captureSessionClicked(CaptureSession &c, bool doubleClick, bool updateDeviceSelection = true);
        void focusSessionClicked(FocusSession &c, bool doubleClick, bool updateDeviceSelection = true);
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
        // Shows the hovered row's full device name(s) as a tooltip, since
        // the row label itself is now a short "Cam N"/"Foc N" form (see
        // deviceShortLabel()).
        void timelineMouseMove(QMouseEvent *event);
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
        void highlightTimelineItem(const Session &session);
        void unhighlightTimelineItem();

        // Tied to the keyboard shortcuts that go to the next or previous
        // items on the timeline. next==true means next, otherwise previous.
        void changeTimelineItem(bool next);
        // These are assigned to various keystrokes.
        void nextTimelineItem();
        void previousTimelineItem();

        // Resolve an optical train name to the device name of the camera/focuser
        // currently assigned to it, so that renamed/reassigned trains don't
        // change the meaning of historical .analyze logs. Falls back to the
        // train name itself if no live device is found.
        QString resolveCameraDevice(const QString &trainname);
        QString resolveFocuserDevice(const QString &trainname);

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
                    const double time, double startTime, const QString &device = "");
        void addTemperature(double temperature, const double time, const QString &device = "");
        void addFocusPosition(double focusPosition, double time, const QString &device = "");
        void addTargetDistance(double targetDistance, const double time);

        // Initialize the graphs (axes, linestyle, pen, name, checkbox callbacks).
        // Returns the graph index.
        int initGraph(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                      const QColor &color, const QString &name);
        // deviceGraphs, when given, receives this call's graph under the ""
        // key (see deviceStatGraph()), and the checkbox this stat is tied to
        // toggles every device's line for it together, not just this one.
        template <typename Func>
        int initGraphAndCB(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                           const QColor &color, const QString &name, const QString &shortName,
                           QCheckBox *cb, Func setCb, QLineEdit *out = nullptr,
                           QMap<QString, int> *deviceGraphs = nullptr);

        // Make graphs visible/invisible & add/delete them from the legend.
        void toggleGraph(int graph_id, bool show);

        // Returns device's line within a per-device stat (HFR, temperature,
        // etc.), sharing baseGraph's axis/line style/checkbox visibility,
        // creating a new line -- with a color variation so overlaid devices
        // stay distinguishable -- on first use.
        int deviceStatGraph(QMap<QString, int> &deviceGraphs, const QString &device, int baseGraph);

        // Initializes the main QCustomPlot windows.
        void initStatsPlot();
        void initTimelinePlot();
        void initGraphicsPlot();
        void initInputSelection();

        // Displays the focus positions and HFRs on the graphics plot.
        void displayFocusGraphics(const QVector<double> &positions, const QVector<double> &hfrs, const bool useWeights,
                                  const QVector<double> &weights, const QVector<bool> &outliers, const QString &curve, const QString &title, bool success);
        // Displays the guider ra and dec drift plot, and computes RMS errors.
        void displayGuideGraphics(double start, double end, double *raRMS,
                                  double *decRMS, double *totalRMS, int *numSamples);

        // Updates the stats value display boxes next to their checkboxes.
        void updateStatsValues();
        // Manages the statsPlot cursor.
        void setStatsCursor(double time);
        void removeStatsCursor();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        void keepCurrent(Qt::CheckState state);
#else
        void keepCurrent(int state);
#endif

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
        // If discoverDevicesOnly is true, this only registers each line's
        // device (so the Timeline's rows can be fully laid out before
        // anything is plotted) instead of actually processing the line; see
        // readDataFromFile()'s two-pass read.
        double processInputLine(const QString &line, bool discoverDevicesOnly = false);

        // Opens a FITS file for viewing.
        void displayFITS(const QString &filename);

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

        // Cached common left margin for the Timeline/Stats plots (see replot()).
        // Only recomputed when m_marginsDirty is set, instead of on every
        // replot() call, since re-measuring against the currently displayed
        // (auto-scaled) tick labels made the axis visibly tremble as their
        // digit count varied from one routine update to the next.
        int m_statsLeftMargin { 0 };
        bool m_marginsDirty { true };

        // The Stats legend's items land in it in whatever order
        // addToLegend() got called for them -- a per-device graph is only
        // created (and added) once its first data point arrives, and
        // re-checking a checkbox re-adds its entry at the end -- so the
        // legend can end up in an arbitrary, shuffled order. sortStatsLegend()
        // (called from replot() when this is set) puts every currently
        // shown entry back in a fixed order matching how the stats are
        // defined in initStatsPlot(), without changing which ones are shown.
        bool m_legendDirty { true };
        void sortStatsLegend();

        void startYAxisTool(QObject *key, const YAxisInfo &info);

        // Map connecting QLineEdits to Y-Axes, so when a QLineEdit is double clicked,
        // the corresponding y-axis can be found.
        std::map<QObject*, YAxisInfo> yAxisMap;

        // Holds the design font size for the stat QLineEdit.
        std::map<QObject*, float> statsFontMap;

        // The .analyze log file being written.
        QString logFilename { "" };
        QSharedPointer<QFile> logFile;
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
        QSharedPointer<FITSViewer> fitsViewer;
        int fitsViewerTabID { 0 };

        // The vertical line in the stats plot.
        QCPItemLine *statsCursor { nullptr };
        QCPItemLine *timelineCursor { nullptr };
        double statsCursorTime { -1 };

        // Keeps the directory from the last time the user loaded a .analyze file.
        QUrl dirPath;
        // This is the .analyze file we're displaying, if we're not displaying the currently active session.
        QUrl displayedSession;
        QString getNextFile(bool after);

        // Display other .analyze files.
        void nextFile();
        void prevFile();
        void displayFile(const QUrl &url, bool forceCurrentSession = false);

        // True if Analyze is displaying data as it comes in from the other modules.
        // False if Analyze is displaying data read from a file.
        bool runtimeDisplay { true };

        // When a module's session is ongoing, we represent it as a "temporary session"
        // which will be replaced once the session is done.
        // Capture and Focus support multiple concurrent devices (multiple camera/
        // focuser tabs), so their in-flight state, including the temporary session,
        // is tracked per device below instead of as a single shared instance.
        GuideSession temporaryGuideSession;
        AlignSession temporaryAlignSession;
        MountSession temporaryMountSession;
        MountFlipSession temporaryMountFlipSession;
        SchedulerJobSession temporarySchedulerJobSession;

        // Per-device capture state-machine variables, keyed by resolved device
        // name (see resolveCameraDevice()). A device with no entry is idle.
        struct CaptureDeviceState
        {
            double startedTime { -1 };
            QString startedFilter;
            CaptureSession temporarySession;
        };
        QMap<QString, CaptureDeviceState> captureDeviceStates;
        // True if any device currently has a capture in flight. Used by guiding,
        // which isn't itself per-device, to decide whether to plot capture RMS.
        bool anyCaptureInProgress() const;

        // previousCaptureStartedTime/previousCaptureCompletedTime track the most
        // recently completed capture across all devices (used to place the
        // Scheduler-driven target-distance graph, which has no device of its own).
        double previousCaptureStartedTime { 1 };
        double previousCaptureCompletedTime { 1 };

        // Per-device autofocus state-machine variables, keyed by resolved device
        // name (see resolveFocuserDevice()). A device with no entry is idle.
        struct FocusDeviceState
        {
            double startedTime { -1 };
            QString startedFilter;
            double startedTemperature { 0 };
            AutofocusReason startedReason { AutofocusReason::FOCUS_NONE };
            QString startedReasonInfo;
            FocusSession temporarySession;
        };
        QMap<QString, FocusDeviceState> focusDeviceStates;

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

        void setSelectedSession(const Session &s);
        void clearSelectedSession();
        Session m_selectedSession;

        // The camera whose per-device Capture stats (HFR, #SubStars,
        // median, eccentricity) are shown in the readout boxes, kept in
        // sync both ways with captureDeviceCombo: selecting a device from
        // the combo or clicking one of its Timeline sessions both call this.
        // Empty with 0-1 cameras (captureDeviceCombo is hidden then; see
        // buildDeviceRows()), in which case updateStatsValues() falls back
        // to whatever Timeline session is currently selected, as before.
        QString m_selectedCaptureDevice;
        void selectCaptureDevice(const QString &device);

        // As m_selectedCaptureDevice/selectCaptureDevice() above, but for
        // the focuser whose temperature/focus-position stats are shown
        // (focusDeviceCombo, kept in sync with clicking a Timeline Focus
        // session).
        QString m_selectedFocusDevice;
        void selectFocusDevice(const QString &device);

        // Analyze log file info.
        QStringList m_LogText;

        // Y-offsets for the timeline plot, computed by buildDeviceRows()
        // rather than fixed at compile time (see captureRowForDevice/
        // focusRowForDevice below for the Capture/Focus rows).
        int ALIGN_Y { 3 };
        int GUIDE_Y { 4 };
        int MERIDIAN_MOUNT_FLIP_Y { 5 };
        int MOUNT_Y { 6 };
        int SCHEDULER_Y { 7 };
        int LAST_Y { 8 };

        // Row assignment for Capture/Focus, keyed by resolved device name (see
        // resolveCameraDevice()/resolveFocuserDevice()). Historical sessions
        // with no recorded device (pre-2.0 .analyze files) use "" as their key,
        // same as any other device.
        QMap<QString, int> captureRowForDevice;
        QMap<QString, int> focusRowForDevice;

        // Every camera/focuser device that has had a tab in this session,
        // grown by addActiveCamera()/addActiveFocuser() (see
        // Capture::cameraDeviceActive()/FocusModule::focuserDeviceActive()).
        // Never shrinks -- a closed tab's device stays, so its earlier events
        // remain visible. Used by buildDeviceRows() to seed a live session's
        // rows, instead of every device configured in the profile's optical
        // trains whether or not it's actually captured/focused through.
        QStringList m_liveCameraDevices;
        QStringList m_liveFocuserDevices;

        // Short, fixed-form label ("Cam 1", "Foc 2", ..., or "Camera"/
        // "Focuser" when there's only one) for each known device, rebuilt by
        // buildDeviceRows() whenever the device set changes (see there for
        // the numbering rule). Used on the Timeline
        // and in the Stats legend in place of the raw (often long) device
        // name; the device-filter menu shows both, and the Timeline shows
        // the full name as a tooltip (see timelineMouseMove()).
        QMap<QString, QString> m_deviceShortLabel;
        QString deviceShortLabel(const QString &device) const
        {
            return m_deviceShortLabel.value(device, device);
        }

        // Rebuilds the rows above -- from the live device lists above for a
        // live session (see addActiveCamera()/addActiveFocuser()), plus any
        // device already present in captureSessions/captureDeviceStates/
        // focusSessions/focusDeviceStates (e.g. from a loaded .analyze file
        // whose devices aren't part of the current profile) -- and rebuilds
        // the Timeline's row labels/axis range to match. Called whenever the
        // session resets (see reset()) and whenever a new device is seen for
        // the first time (see captureRow()/focusRow()). Devices hidden via
        // the device-filter menu (see
        // isDeviceHidden()) are left out of the row assignment entirely, so
        // shown devices stay packed with no gaps; rebuildDeviceFilterMenu()
        // still gets the full, unfiltered device list so a hidden device
        // remains available to bring back.
        void buildDeviceRows();

        // True for any device the user unchecked in the device-filter menu.
        // "" (the pre-2.0-log/no-device bucket) is never filterable.
        bool isDeviceHidden(const QString &device) const
        {
            return !device.isEmpty() && Options::analyzeHiddenDevices().contains(device);
        }

        // Persists the device's hidden state and redraws the Timeline/Stats
        // plots to match (see reloadDisplay()).
        void setDeviceHidden(const QString &device, bool hidden);

        // (Re)populates deviceFilterButton's menu with one checkable action
        // per known camera/focuser, reflecting the persisted hidden set.
        // Called from buildDeviceRows() with its full, unfiltered device
        // lists.
        void rebuildDeviceFilterMenu(const QStringList &cameraDevices, const QStringList &focuserDevices);
        QMenu *deviceFilterMenu { nullptr };

        // Applies the current hidden-device set to every already-created
        // per-device Stats graph (HFR_GRAPH, TEMPERATURE_GRAPH, etc.).
        // Hiding a device only stops isDeviceHidden()'s process*() guards
        // from feeding it *new* points -- it doesn't touch graphs already
        // created (with a checkbox-derived visibility/legend state) from
        // before it was hidden, so this re-applies that state to all of
        // them. Called from buildDeviceRows(), since that's already the
        // central place the device-filter set gets re-applied.
        void applyDeviceStatVisibility();

        // Forgets every per-device entry (keeping only the "" placeholder)
        // in the six per-device Stats graph maps, hiding and removing each
        // from the legend first. Called from reset(), so switching to a
        // session/file with a different (or no) set of devices doesn't
        // leave the previous one's now-stale entries sitting in the Stats
        // legend -- applyDeviceStatVisibility() only re-applies visibility
        // to whatever's still *in* these maps, it doesn't know a device is
        // stale. The now-unused QCPGraph objects are left in the plot
        // (invisible, dataless) rather than removed: QCustomPlot::
        // removeGraph() shifts every later graph's stored index, which the
        // many other XXX_GRAPH int members throughout this file assume
        // stays fixed once assigned.
        void clearDeviceStatGraphs();

        // Redraws the Timeline/Stats plots from the log backing whatever is
        // currently displayed (a loaded .analyze file, or the current live
        // session's log) so a device-filter change takes effect immediately.
        // A hidden device is skipped while replaying (see the process*()
        // functions' isDeviceHidden() guards) rather than removed from the
        // log itself, so showing it again later recovers its full history.
        void reloadDisplay();

        // Returns the row for this device, allocating one (and rebuilding all
        // rows below it) on first use if the device hasn't been seen yet.
        int captureRow(const QString &device)
        {
            auto it = captureRowForDevice.constFind(device);
            if (it != captureRowForDevice.constEnd())
                return it.value();
            buildDeviceRows();
            return captureRowForDevice.value(device, 1);
        }
        int focusRow(const QString &device)
        {
            auto it = focusRowForDevice.constFind(device);
            if (it != focusRowForDevice.constEnd())
                return it.value();
            buildDeviceRows();
            return focusRowForDevice.value(device, 1);
        }
        bool isCaptureRow(int row) const
        {
            return captureRowForDevice.values().contains(row);
        }
        bool isFocusRow(int row) const
        {
            return focusRowForDevice.values().contains(row);
        }
        // Which device(s) a Capture/Focus row corresponds to (normally just
        // one, since every known device gets its own row).
        QStringList captureDevicesForRow(int row) const;
        QStringList focusDevicesForRow(int row) const;

        // Error bars used on the Focus graphs
        QCPErrorBars *errorBars = nullptr;
        QCPErrorBars *finalErrorBars = nullptr;
};
}


#endif // Analyze
