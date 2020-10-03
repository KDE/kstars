/*  Ekos Analyze Module
    Copyright (C) 2020 Hy Murveit <hy@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef ANALYZE_H
#define ANALYZE_H

#include <QtDBus>

#include "ekos/ekos.h"
#include "ekos/mount/mount.h"
#include "indi/inditelescope.h"
#include "ui_analyze.h"

class FITSViewer;
class OffsetDateTimeTicker;

namespace Ekos
{
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

                // These 2 are used to build html tables for the info box display.
                void startTable(const QString &name, const QString &status,
                                const QDateTime &startClock, const QDateTime &endClock);
                void addRow(const QString &key, const QString &value1String, const QString &value2String = "");
                // Returns the html string to use in the info box.
                QString html() const;

                // True if this session is temporary.
                bool isTemporary() const;

            private:
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
                      duration(duration_), filter(filter_) {}
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
                ISD::Telescope::Status state;
                MountSession(double start_, double end_, QCPItemRect *rect, ISD::Telescope::Status state_)
                    : Session(start_, end_, MOUNT_Y, rect), state(state_) {}
                MountSession() : Session(0, 0, MOUNT_Y, nullptr) {}
        };
        class MountFlipSession : public Session
        {
            public:
                Mount::MeridianFlipStatus state;
                MountFlipSession(double start_, double end_, QCPItemRect *rect, Mount::MeridianFlipStatus state_)
                    : Session(start_, end_, MERIDIAN_FLIP_Y, rect), state(state_) {}
                MountFlipSession() : Session(0, 0, MERIDIAN_FLIP_Y, nullptr) {}
        };
        class FocusSession : public Session
        {
            public:
                bool success;
                double temperature;
                QString filter;
                QString points;
                QVector<double> positions; // Double to be more friendly to QCustomPlot addData.
                QVector<double> hfrs;
                FocusSession() : Session(0, 0, FOCUS_Y, nullptr) {}
                FocusSession(double start_, double end_, QCPItemRect *rect, bool ok, double temperature_,
                             const QString &filter_, const QString &points_);
        };

    public slots:
        // These slots are messages received from the different Ekos processes
        // used to gather data about those processes.

        // From Capture
        void captureComplete(const QString &filename, double exposureSeconds,
                             const QString &filter, double hfr);
        void captureStarting(double exposureSeconds, const QString &filter);
        void captureAborted(double exposureSeconds);

        // From Guide
        void guideState(Ekos::GuideState status);
        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

        // From Focus
        void autofocusStarting(double temperature, const QString &filter);
        void autofocusComplete(const QString &filter, const QString &points);
        void autofocusAborted(const QString &filter, const QString &points);

        // From Align
        void alignState(Ekos::AlignState state);

        // From Mount
        void mountState(ISD::Telescope::Status status);
        void mountCoords(const QString &ra, const QString &dec, const QString &az,
                         const QString &alt, int pierSide, const QString &ha);
        void mountFlipStatus(Ekos::Mount::MeridianFlipStatus status);

    private slots:

    signals:

    private:

        // The file-reading, processInputLine(), and signal-slot codepaths share the methods below
        // to process their messages. Time is the offset in seconds from the start of the log.
        // BatchMode is true in the file reading path. It means don't call replot() as there may be
        // many more messages to come. The rest of the args are specific to the message type.
        void processCaptureStarting(double time, double exposureSeconds, const QString &filter, bool batchMode = false);
        void processCaptureComplete(double time, const QString &filename,
                                    double exposureSeconds, const QString &filter, double hfr, bool batchMode = false);
        void processCaptureAborted(double time, double exposureSeconds, bool batchMode = false);
        void processAutofocusStarting(double time, double temperature, const QString &filter, bool batchMode = false);
        void processAutofocusComplete(double time, const QString &filter, const QString &points, bool batchMode = false);
        void processAutofocusAborted(double time, const QString &filter, const QString &points, bool batchMode = false);
        void processGuideState(double time, const QString &state, bool batchMode = false);
        void processGuideStats(double time, double raError, double decError, int raPulse,
                               int decPulse, double snr, double skyBg, int numStars, bool batchMode = false);
        void processMountCoords(double time, double ra, double dec, double az, double alt,
                                int pierSide, double ha, bool batchMode = false);

        void processMountState(double time, const QString &statusString, bool batchMode = false);
        void processAlignState(double time, const QString &statusString, bool batchMode = false);
        void processMountFlipState(double time, const QString &statusString, bool batchMode = false);

        // Plotting primatives.
        void replot(bool adjustSlider = true);
        void zoomIn();
        void zoomOut();
        void scroll(int value);
        void scrollRight();
        void scrollLeft();

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

        // Low-level callbacks.
        // These two call processTimelineClick().
        void timelineMousePress(QMouseEvent *event);
        void timelineMouseDoubleClick(QMouseEvent *event);
        // Calls zoomIn or zoomOut.
        void timelineMouseWheel(QWheelEvent *event);

        void processStatsClick(QMouseEvent *event, bool doubleClick);
        void statsMousePress(QMouseEvent *event);
        void statsMouseDoubleClick(QMouseEvent *event);
        void statsMouseMove(QMouseEvent *event);
        void setupKeyboardShortcuts(QCustomPlot *plot);

        // (Un)highlights a segment on the timeline after one is clicked.
        // This indicates which segment's data is displayed in the
        // graphicsPlot and infoBox.
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
        void addHFR(double hfr, const double time, double startTime);

        // AddGuideStats uses rmsFilter() to compute RMS values of the squared
        // RA and DEC errors, thus calculating the RMS error.
        double rmsFilter(double x);
        void initRmsFilter();
        void resetRmsFilter();

        // Initialize the graphs (axes, linestyle, pen, name, checkbox callbacks).
        // Returns the graph index.
        int initGraph(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                      const QColor &color, const QString &name);
        template <typename Func>
        int initGraphAndCB(QCustomPlot *plot, QCPAxis *yAxis, QCPGraph::LineStyle lineStyle,
                           const QColor &color, const QString &name, QCheckBox *cb, Func setCb);

        // Make graphs visible/invisible & add/delete them from the legend.
        void toggleGraph(int graph_id, bool show);

        // Initializes the main QCustomPlot windows.
        void initStatsPlot();
        void initTimelinePlot();
        void initGraphicsPlot();
        void initInputSelection();

        // Displays the focus positions and HFRs on the graphics plot.
        void displayFocusGraphics(const QVector<double> &positions, const QVector<double> &hfrs, bool success);
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

        // Digital filter values for the RMS filter.
        double rmsFilterAlpha { 0 };
        double filteredRMS { 0 };

        // Y-axes for the for several plots where we rescale based on data.
        // QCustomPlot owns these pointers' memory, don't free it.
        QCPAxis *snrAxis;
        QCPAxis *numStarsAxis;
        QCPAxis *skyBgAxis;
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

        // Capture state-machine variables.
        double captureStartedTime { -1 };
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
        int numStarsMax { 0 };
        double snrMax { 0 };
        double skyBgMax { 0 };

        // AlignState state-machine variables.
        AlignState lastAlignStateReceived { ALIGN_IDLE };
        AlignState lastAlignStateStarted { ALIGN_IDLE };
        double lastAlignStateStartedTime { -1 };

        // MountState state-machine variables.
        double mountStateStartedTime { -1 };
        ISD::Telescope::Status lastMountState { ISD::Telescope::Status::MOUNT_IDLE };

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
        Mount::MeridianFlipStatus lastMountFlipStateReceived { Mount::FLIP_NONE};
        Mount::MeridianFlipStatus lastMountFlipStateStarted { Mount::FLIP_NONE };
        double mountFlipStateStartedTime { -1 };

        // Y-offsets for the timeline plot for the various modules.
        static constexpr int CAPTURE_Y = 1;
        static constexpr int FOCUS_Y = 2;
        static constexpr int ALIGN_Y = 3;
        static constexpr int GUIDE_Y = 4;
        static constexpr int MERIDIAN_FLIP_Y = 5;
        static constexpr int MOUNT_Y = 6;
        static constexpr int LAST_Y = 7;
};
}


#endif // Analyze
