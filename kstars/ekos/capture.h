/*  Ekos Capture tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include "capture.h"

#include <QTimer>
#include <QUrl>
#include <QtDBus/QtDBus>

#include "ui_capture.h"

#include "fitsviewer/fitscommon.h"
#include "indi/indistd.h"
#include "indi/indiccd.h"
#include "indi/inditelescope.h"

class QProgressIndicator;
class QTableWidgetItem;
class KDirWatch;

/**
 *@namespace Ekos
 *@short Ekos is an advanced Astrophotography tool for Linux.
 * It is based on a modular extensible framework to perform common astrophotography tasks. This includes highly accurate GOTOs using astrometry solver, ability to measure and correct polar alignment errors ,
 * auto-focus & auto-guide capabilities, and capture of single or stack of images with filter wheel support.\n
 * Features:
 * - Control your telescope, CCD (& DSLRs), filter wheel, focuser, guider, adaptive optics unit, and any INDI-compatible auxiliary device from Ekos.
 * - Extremely accurate GOTOs using astrometry.net solver (both Online and Offline solvers supported).
 * - Load & Slew: Load a FITS image, slew to solved coordinates, and center the mount on the exact image coordinates in order to get the same desired frame.
 * - Measure & Correct Polar Alignment errors using astromety.net solver.
 * - Auto and manual focus modes using Half-Flux-Radius (HFR) method.
 * - Automated unattended meridian flip. Ekos performs post meridian flip alignment, calibration, and guiding to resume the capture session.
 * - Automatic focus between exposures when a user-configurable HFR limit is exceeded.
 * - Auto guiding with support for automatic dithering between exposures and support for Adaptive Optics devices in addition to traditional guiders.
 * - Powerful sequence queue for batch capture of images with optional prefixes, timestamps, filter wheel selection, and much more!
 * - Export and import sequence queue sets as Ekos Sequence Queue (.esq) files.
 * - Center the telescope anywhere in a captured FITS image or any FITS with World Coordinate System (WCS) header.
 * - Automatic flat field capture, just set the desired ADU and let Ekos does the rest!
 * - Automatic abort and resumption of exposure tasks if guiding errors exceed a user-configurable value.
 * - Support for dome slaving.
 * - Complete integration with KStars Observation Planner and SkyMap
 * - Integrate with all INDI native devices.
 * - Powerful scripting capabilities via \ref EkosDBusInterface "DBus."
 *
 * The primary class is EkosManager. It handles startup and shutdown of local and remote INDI devices, manages and orchesterates the various Ekos modules, and provides advanced DBus
 * interface to enable unattended scripting.
*@author Jasem Mutlaq
 *@version 1.1
 */
namespace Ekos
{

class SequenceJob : public QObject
{
    Q_OBJECT    

    public:

    typedef enum { JOB_IDLE, JOB_BUSY, JOB_ERROR, JOB_ABORTED, JOB_DONE } JOBStatus;
    typedef enum { CAPTURE_OK, CAPTURE_FRAME_ERROR, CAPTURE_BIN_ERROR, CAPTURE_FOCUS_ERROR} CAPTUREResult;

    SequenceJob();

    CAPTUREResult capture(bool isDark=false);
    void reset();
    void abort();
    void done();
    void prepareCapture();

    JOBStatus getStatus() { return status; }
    const QString & getStatusString() { return statusStrings[status]; }
    bool isPreview() { return preview;}
    int getDelay() { return delay;}
    int getCount() { return count;}
    unsigned int getCompleted() { return completed; }
    const QString & getPrefix() { return prefix;}
    double getExposure() const { return exposure;}

    void setActiveCCD(ISD::CCD *ccd) { activeCCD = ccd; }
    ISD::CCD *getActiveCCD() { return activeCCD;}

    void setActiveFilter(ISD::GDInterface *filter) { activeFilter = filter; }
    ISD::GDInterface *getActiveFilter() { return activeFilter;}

    void setActiveChip(ISD::CCDChip *chip) { activeChip = chip; }
    ISD::CCDChip *getActiveChip() { return activeChip;}

    void setFITSDir(const QString &dir) { fitsDir = dir;}
    const QString & getFITSDir() { return fitsDir; }

    void setTargetFilter(int pos, const QString & name);
    int getTargetFilter() { return targetFilter;}
    int getCurrentFilter() const;
    void setCurrentFilter(int value);

    const QString &getFilterName() { return filter; }
    void setFrameType(int type, const QString & name);
    int getFrameType() { return frameType;}
    void setCaptureFilter(FITSScale capFilter) { captureFilter = capFilter; }
    void setISOMode(bool mode) { isoMode = mode; }
    bool getISOMode() { return isoMode;}
    void setPreview(bool enable) { preview = enable; }
    void setShowFITS(bool enable) { showFITS = enable; }
    bool isShowFITS() { return showFITS;}
    void setPrefix(const QString &cprefix) { prefix = cprefix;}
    void setFrame(int in_x, int in_y, int in_w, int in_h) { x=in_x; y=in_y; w=in_w; h=in_h; }
    int getSubX() { return x;}
    int getSubY() { return y;}
    int getSubW() { return w;}
    int getSubH() { return h;}
    void setBin(int xbin, int ybin) { binX = xbin; binY=ybin;}
    int getXBin() { return binX; }
    int getYBin() { return binY; }
    void setDelay(int in_delay) { delay = in_delay; }
    void setCount(int in_count) { count = in_count;}
    void setImageType(int type) { imageType = type;}
    void setExposure(double duration) { exposure = duration;}
    void setStatusCell(QTableWidgetItem *cell) { statusCell = cell; }
    void setCompleted(unsigned int in_completed) { completed = in_completed;}
    int getISOIndex() const;
    void setISOIndex(int value);

    double getExposeLeft() const;
    void setExposeLeft(double value);
    void resetStatus();
    void setPrefixSettings(const QString &prefix, bool typeEnabled, bool filterEnabled, bool exposureEnabled);
    void getPrefixSettings(QString &prefix, bool &typeEnabled, bool &filterEnabled, bool &exposureEnabled);

    double getCurrentTemperature() const;
    void setCurrentTemperature(double value);

    double getTargetTemperature() const;
    void setTargetTemperature(double value);    

    double getTargetADU() const;
    void setTargetADU(double value);

    int getCaptureRetires() const;
    void setCaptureRetires(int value);

signals:
    void prepareComplete();

private:

    QStringList statusStrings;
    ISD::CCDChip *activeChip;
    ISD::CCD *activeCCD;
    ISD::GDInterface *activeFilter;

    double exposure;
    int frameType;
    QString frameTypeName;
    int targetFilter;
    int currentFilter;

    QString filter;
    int imageType;
    int binX, binY;
    int x,y,w,h;
    QString prefix;
    int count;
    int delay;    
    bool isoMode;
    bool preview;
    bool showFITS;
    bool filterReady, temperatureReady;
    int isoIndex;
    int captureRetires;
    unsigned int completed;
    double exposeLeft;
    double currentTemperature, targetTemperature;
    FITSScale captureFilter;
    QTableWidgetItem *statusCell;
    QString fitsDir;

    bool typePrefixEnabled, filterPrefixEnabled, expPrefixEnabled;
    QString rawPrefix;

    JOBStatus status;

    double targetADU;
};

/**
 *@class Capture
 *@short Captures single or sequence of images from a CCD.
 * The capture class support capturing single or multiple images from a CCD, it provides a powerful sequence queue with filter wheel selection. Any sequence queue can be saved as Ekos Sequence Queue (.esq).
 * All image capture operations are saved as Sequence Jobs that encapsulate all the different options in a capture process. The user may select in sequence autofocusing by setting a maximum HFR limit. When the limit
 * is exceeded, it automatically trigger autofocus operation. The capture process can also be linked with guide module. If guiding deviations exceed a certain threshold, the capture operation aborts until
 * the guiding deviation resume to acceptable levels and the capture operation is resumed.
 *@author Jasem Mutlaq
 *@version 1.2
 */
class Capture : public QWidget, public Ui::Capture
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Capture")

public:

    enum { CALIBRATE_NONE, CALIBRATE_START, CALIBRATE_DONE };
    typedef enum { MF_NONE, MF_INITIATED, MF_FLIPPING, MF_SLEWING, MF_ALIGNING, MF_GUIDING } MFStage;

    Capture();
    ~Capture();

    /** @defgroup CaptureDBusInterface Ekos DBus Interface - Capture Module
     * Ekos::Capture interface provides advanced scripting capabilities to capture image sequences.
    */

    /*@{*/

    /** DBUS interface function.
     * select the CCD device from the available CCD drivers.
     * @param device The CCD device name
     */
    Q_SCRIPTABLE bool setCCD(QString device);

    /** DBUS interface function.
     * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
     * @param device The filter device name
     */
    Q_SCRIPTABLE bool setFilter(QString device, int filterSlot);

    /** DBUS interface function.
     * Aborts any current jobs and remove all sequence queue jobs.
     */
    Q_SCRIPTABLE Q_NOREPLY void clearSequenceQueue();

    /** DBUS interface function.
     * Returns the overall sequence queue status. If there are no jobs pending, it returns "Invalid". If all jobs are idle, it returns "Idle". If all jobs are complete, it returns "Complete". If one or more jobs are aborted
     * it returns "Aborted". If one or more jobs have errors, it returns "Error". If any jobs is under progress, returns "Running".
     */
    Q_SCRIPTABLE QString getSequenceQueueStatus();

    /** DBUS interface function.
     * Loads the Ekos Sequence Queue file in the Sequence Queue. Jobs are appended to existing jobs.
     * @param fileURL full URL of the filename
     */
    Q_SCRIPTABLE bool loadSequenceQueue(const QUrl &fileURL);

    /** DBUS interface function.
     * Enables or disables the maximum guiding deviation and sets its value.
     * @param enable If true, enable the guiding deviation check, otherwise, disable it.
     * @param if enable is true, it sets the maximum guiding deviation in arcsecs. If the value is exceeded, the capture operation is aborted until the value falls below the value threshold.
     */
    Q_SCRIPTABLE Q_NOREPLY void setMaximumGuidingDeviaiton(bool enable, double value);

    /** DBUS interface function.
     * Enables or disables the in sequence focus and sets Half-Flux-Radius (HFR) limit.
     * @param enable If true, enable the in sequence auto focus check, otherwise, disable it.
     * @param if enable is true, it sets HFR in pixels. After each exposure, the HFR is re-measured and if it exceeds the specified value, an autofocus operation will be commanded.
     */
    Q_SCRIPTABLE Q_NOREPLY void setInSequenceFocus(bool enable, double HFR);

    /** DBUS interface function.
     * Enables or disables park on complete option.
     * @param enable If true, mount shall be commanded to parking position after all jobs are complete in the sequence queue.
     */
    Q_SCRIPTABLE Q_NOREPLY void setParkOnComplete(bool enable);

    /** DBUS interface function.
     * Enable or Disable meridian flip, and sets its activation hour.
     * @param enable If true, meridian flip will be command after user-configurable number of hours.
     */
    Q_SCRIPTABLE Q_NOREPLY void setMeridianFlip(bool enable);

    /** DBUS interface function.
     * Sets meridian flip trigger hour.
     * @param hours Number of hours after the meridian at which the mount is commanded to flip.
     */
    Q_SCRIPTABLE Q_NOREPLY void setMeridianFlipHour(double hours);

    /** DBUS interface function.
     * Does the CCD has a cooler control (On/Off) ?
     */
    Q_SCRIPTABLE bool hasCoolerControl();

    /** DBUS interface function.
     * Set the CCD cooler ON/OFF
     *
     */
    Q_SCRIPTABLE bool setCoolerControl(bool enable);

    /** DBUS interface function.
     * @return Returns the number of jobs in the sequence queue.
     */
    Q_SCRIPTABLE int            getJobCount() { return jobs.count(); }

    /** DBUS interface function.
     * @param id job number. Job IDs start from 0 to N-1.
     * @return Returns the job state (Idle, In Progress, Error, Aborted, Complete)
     */
    Q_SCRIPTABLE QString        getJobState(int id);

    /** DBUS interface function.
     * @param id job number. Job IDs start from 0 to N-1.
     * @return Returns The number of images completed capture in the job.
     */
    Q_SCRIPTABLE int            getJobImageProgress(int id);

    /** DBUS interface function.
     * @param id job number. Job IDs start from 0 to N-1.
     * @return Returns the total number of images to capture in the job.
     */
    Q_SCRIPTABLE int            getJobImageCount(int id);

    /** DBUS interface function.
     * @param id job number. Job IDs start from 0 to N-1.
     * @return Returns the number of seconds left in an exposure operation.
     */
    Q_SCRIPTABLE double         getJobExposureProgress(int id);

    /** DBUS interface function.
     * @param id job number. Job IDs start from 0 to N-1.
     * @return Returns the total requested exposure duration in the job.
     */
    Q_SCRIPTABLE double         getJobExposureDuration(int id);

    /** DBUS interface function.
     * Clear in-sequence focus settings. It sets the autofocus HFR to zero so that next autofocus value is remembered for the in-sequence focusing.
     */
    Q_SCRIPTABLE Q_NOREPLY  void clearAutoFocusHFR();

    /** @}*/

    void addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD);
    void addFilter(ISD::GDInterface *newFilter);
    void addGuideHead(ISD::GDInterface *newCCD);
    void syncFrameType(ISD::GDInterface *ccd);
    void setTelescope(ISD::GDInterface *newTelescope);
    void syncTelescopeInfo();
    void syncFilterInfo();

    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

    /* Capture */
    void updateSequencePrefix( const QString &newPrefix, const QString &dir);
public slots:

    /* Capture */
    /** DBUS interface function.
     * Starts the sequence queue capture procedure sequentially by starting all jobs that are either Idle or Aborted in order.
     */
    Q_SCRIPTABLE Q_NOREPLY void start();

    /** DBUS interface function.
     * Aborts all jobs and set current job status to Aborted if it was In Progress.
     */
    Q_SCRIPTABLE Q_NOREPLY void abort();

    /**
     * @brief captureOne Capture one preview image
     */
    void captureOne();

    /**
     * @brief captureImage Initiates image capture in the active job.
     */
    void captureImage();

    /**
     * @brief newFITS process new FITS data received from camera. Update status of active job and overall sequence.
     * @param bp pointer to blob contianing FITS data
     */
    void newFITS(IBLOB *bp);

    /**
     * @brief checkCCD Refreshes the CCD information in the capture module.
     * @param CCDNum The CCD index in the CCD combo box to select as the active CCD.
     */
    void checkCCD(int CCDNum=-1);

    /**
     * @brief checkFilter Refreshes the filter wheel information in the capture module.
     * @param filterNum The filter wheel index in the filter device combo box to set as the active filter.
     */
    void checkFilter(int filterNum=-1);

    /**
     * @brief processCCDNumber Process number properties arriving from CCD. Currently, only CCD and Guider frames are processed.
     * @param nvp pointer to number property.
     */
    void processCCDNumber(INumberVectorProperty *nvp);

    /**
     * @brief processTelescopeNumber Process number properties arriving from telescope for meridian flip purposes.
     * @param nvp pointer to number property.
     */
    void processTelescopeNumber(INumberVectorProperty *nvp);

    /**
     * @brief addJob Add a new job to the sequence queue given the settings in the GUI.
     * @param preview True if the job is a preview job, otherwise, it is added as a batch job.
     */
    void addJob(bool preview=false);

    /**
     * @brief removeJob Remove a job from the currently selected row. If no row is selected, it remove the last job in the queue.
     */
    void removeJob();

    /**
     * @brief moveJobUp Move the job in the sequence queue one place up.
     */
    void moveJobUp();

    /**
     * @brief moveJobDown Move the job in the sequence queue one place down.
     */
    void moveJobDown();

    /**
     * @brief enableGuideLimits Enable guide deviation check box and guide deviation limits spin box.
     */
    void enableGuideLimits();

    /**
     * @brief setGuideDeviation Set the guiding deviaiton as measured by the guiding module. Abort capture if deviation exceeds user value. Resume capture if capture was aborted and guiding deviations are below user value.
     * @param delta_ra Deviation in RA in arcsecs from the selected guide star.
     * @param delta_dec Deviation in DEC in arcsecs from the selected guide star.
     */
    void setGuideDeviation(double delta_ra, double delta_dec);

    /**
     * @brief setGuideDither Set whether dithering is enable/disabled in guiding module.
     * @param enable True if dithering is enabled, false otherwise.
     */
    void setGuideDither(bool enable);

    /**
     * @brief setAutoguiding Set autoguiding status from guiding module.
     * @param enable True if autoguiding is enabled and running, false otherwise.
     * @param isDithering true if dithering is enabled.
     */
    void setAutoguiding(bool enable, bool isDithering);

    /**
     * @brief resumeCapture Resume capture after dither and/or focusing processes are complete.
     */
    void resumeCapture();

    /**
     * @brief checkPreview When "Display in FITS Viewer" button is toggled, enable/disable preview button accordingly since preview only works if we can display in the FITS Viewer.
     * @param enable True if "Display in FITS Viewer" checkbox is true, false otherwise.
     */
    void checkPreview(bool enable);

    /**
     * @brief updateCCDTemperature Update CCD temperature in capture module.
     * @param value Temperature in celcius.
     */
    void updateCCDTemperature(double value);

    /**
     * @brief setTemperature Set CCD temperature from the user GUI settings.
     */
    void setTemperature();

    /**
     * @brief setDirty Set dirty bit to indicate sequence queue file was modified and needs saving.
     */
    void setDirty();

    void checkFrameType(int index);
    void resetFrame();
    void updateFocusStatus(bool status);
    void updateAutofocusStatus(bool status, double HFR);
    void updateCaptureProgress(ISD::CCDChip *tChip, double value, IPState state);
    void checkSeqBoundary(const QString &path);   
    void saveFITSDirectory();

    void setGuideChip(ISD::CCDChip* chip) { guideChip = chip; }

    void loadSequenceQueue();
    void saveSequenceQueue();
    void saveSequenceQueueAs();

    void resetJobs();
    void editJob(QModelIndex i);
    void resetJobEdit();
    void executeJob();

    void checkMeridianFlipTimeout();
    void checkAlignmentSlewComplete();
    void enableAlignmentFlag();

signals:
        void newLog();
        void exposureComplete();
        void checkFocus(double);
        void telescopeParking();
        void suspendGuiding(bool);
        void meridianFlipStarted();
        void meridialFlipTracked();
        void meridianFlipCompleted();

private:

    bool resumeSequence();
    void startNextExposure();
    void updateFrameProperties();
    void prepareJob(SequenceJob *job);
    bool processJobInfo(XMLEle *root);    
    bool saveSequenceQueue(const QString &path);
    void constructPrefix(QString &imagePrefix);
    double setCurrentADU(double value);

    /* Meridian Flip */
    bool checkMeridianFlip();
    void checkGuidingAfterFlip();
    double getCurrentHA();

    /* Capture */
    KDirWatch          *seqWatcher;
    double	seqExpose;
    int	seqTotalCount;
    int	seqCurrentCount;
    int	seqDelay;
    int     retries;
    QTimer *seqTimer;
    QString		seqPrefix;
    int			seqCount;

    int calibrationState;
    bool useGuideHead;

    SequenceJob *activeJob;

    QList<ISD::CCD *> CCDs;

    ISD::CCDChip *targetChip;
    ISD::CCDChip *guideChip;

    // They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
    QList<ISD::GDInterface *> Filters;

    QList<SequenceJob *> jobs;

    ISD::Telescope *currentTelescope;
    ISD::CCD *currentCCD;
    ISD::GDInterface *currentFilter;

    ITextVectorProperty *filterName;
    INumberVectorProperty *filterSlot;

    QStringList logText;
    QUrl sequenceURL;
    bool mDirty;
    bool jobUnderEdit;
    int currentFilterPosition;
    QProgressIndicator *pi;

    // Guide Deviation
    bool deviationDetected;
    bool spikeDetected;

    // Dither
    bool guideDither;
    bool isAutoGuiding;

    // Autofocus
    bool isAutoFocus;
    bool autoFocusStatus;
    bool firstAutoFocus;
    bool isFocusBusy;

    //Meridan flip
    double initialHA;
    double initialRA;
    bool resumeAlignmentAfterFlip;
    bool resumeGuidingAfterFlip;
    MFStage meridianFlipStage;

    // Flat field automation
    double ExpRaw1, ExpRaw2;
    double ADURaw1, ADURaw2;
    double ADUSlope;

    // File HFR
    double fileHFR;

};

}

#endif  // CAPTURE_H
