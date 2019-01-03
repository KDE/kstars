/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#ifdef USE_QT5_INDI
#include <baseclientqt.h>
#else
#include <baseclient.h>
#endif

#include "ui_manager.h"

#include "ekos.h"
#include "align/align.h"
#include "auxiliary/dome.h"
#include "auxiliary/weather.h"
#include "auxiliary/dustcap.h"
#include "capture/capture.h"
#include "focus/focus.h"
#include "guide/guide.h"
#include "indi/indistd.h"
#include "mount/mount.h"
#include "scheduler/scheduler.h"
#include "auxiliary/filtermanager.h"
#include "auxiliary/serialportassistant.h"
#include "ksnotification.h"
// Can't use forward declaration with QPointer. QTBUG-29588
#include "auxiliary/opslogs.h"

#include <QDialog>
#include <QHash>
#include <QtDBus/QtDBus>

#include <memory>

//! Generic record interfaces and implementations.
namespace EkosLive {
  class Client;
  class Message;
  class Media;
}

class QProgressIndicator;
class DriverInfo;
class ProfileInfo;
class KPageWidgetItem;

/**
 * @class Manager
 * @short Primary class to handle all Ekos modules.
 * The Ekos Manager class manages startup and shutdown of INDI devices and registeration of devices within Ekos Modules. Ekos module consist of \ref Ekos::Mount, \ref Ekos::Capture, \ref Ekos::Focus, \ref Ekos::Guide, and \ref Ekos::Align modules.
 * \defgroup EkosDBusInterface "Ekos DBus Interface" provides high level functions to control devices and Ekos modules for a total robotic operation:
 * <ul>
 * <li>\ref CaptureDBusInterface "Capture Module DBus Interface"</li>
 * <li>\ref FocusDBusInterface "Focus Module DBus Interface"</li>
 * <li>\ref MountDBusInterface "Mount Module DBus Interface"</li>
 * <li>\ref GuideDBusInterface "Guide Module DBus Interface"</li>
 * <li>\ref AlignDBusInterface "Align Module DBus Interface"</li>
 * <li>\ref WeatherDBusInterface "Weather DBus Interface"</li>
 * <li>\ref DustCapDBusInterface "Dust Cap DBus Interface"</li>
 * </ul>
 *  For low level access to INDI devices, the \ref INDIDBusInterface "INDI Dbus Interface" provides complete access to INDI devices and properties.
 *  Ekos Manager provides a summary of operations progress in the <i>Summary</i> section of the <i>Setup</i> tab.
 *
 * @author Jasem Mutlaq
 * @version 1.6
 */
namespace Ekos
{

class Manager : public QDialog, public Ui::Manager
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos")

    Q_SCRIPTABLE Q_PROPERTY(CommunicationStatus indiStatus READ indiStatus NOTIFY indiStatusChanged)
    Q_SCRIPTABLE Q_PROPERTY(CommunicationStatus ekosStatus READ ekosStatus NOTIFY ekosStatusChanged)
    Q_SCRIPTABLE Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

  public:
    explicit Manager(QWidget *parent);
    ~Manager();

    void appendLogText(const QString &);
    //void refreshRemoteDrivers();
    void setOptionsWidget(KPageWidgetItem *ops) { ekosOptionsWidget = ops; }
    void addObjectToScheduler(SkyObject *object);

    Guide *guideModule() { return guideProcess.get(); }
    Align *alignModule() { return alignProcess.get(); }
    Mount *mountModule() { return mountProcess.get(); }
    Focus *focusModule() { return focusProcess.get(); }
    Dome  *domeModule() { return domeProcess.get(); }
    DustCap *capModule() { return dustCapProcess.get(); }
    Capture *captureModule() { return captureProcess.get(); }
    FITSView *getSummaryPreview() { return summaryPreview.get(); }
    QString getCurrentJobName();
    void announceEvent(const QString &message, KSNotification::EventType event);

    /**
     * Manager interface provides advanced scripting capabilities to establish and shutdown Ekos services.
     */

    /*@{*/

    /**
     * DBUS interface function.
     * set Current device profile.
     * @param profileName Profile name
     * @return True if profile is set, false if not found.
     */
    Q_SCRIPTABLE bool setProfile(const QString &profileName);

    /**
     * DBUS interface function
     * @brief getProfiles Return a list of all device profiles
     * @return List of device profiles
     */
    Q_SCRIPTABLE QStringList getProfiles();

    /**
     * DBUS interface function.
     * @return INDI connection status (0 Idle, 1 Pending, 2 Connected, 3 Error)
     * @deprecated
     */
    Q_SCRIPTABLE unsigned int getINDIConnectionStatus() { return m_indiStatus; }

    Q_SCRIPTABLE CommunicationStatus indiStatus() { return m_indiStatus; }

    /**
     * DBUS interface function.
     * @return Ekos starting status (0 Idle, 1 Pending, 2 Started, 3 Error)
     * @deprecated
     */
    Q_SCRIPTABLE unsigned int getEkosStartingStatus() { return m_ekosStatus; }

    Q_SCRIPTABLE CommunicationStatus ekosStatus() { return m_ekosStatus; }

    /**
     * DBUS interface function.
     * If connection mode is local, the function first establishes an INDI server with all the specified drivers in Ekos options or as set by the user. For remote connection,
     * it establishes connection to the remote INDI server.
     * @return Returns true if server started successful (local mode) or connection to remote server is successful (remote mode).
     */
    Q_SCRIPTABLE bool start();

    /**
     * DBUS interface function.
     * If connection mode is local, the function terminates the local INDI server and drivers. For remote, it disconnects from the remote INDI server.
     */
    Q_SCRIPTABLE bool stop();

    Q_SCRIPTABLE QStringList logText() { return m_LogText; }

 signals:
    // Have to use full Ekos::CommunicationStatus for DBus signal to work
    void ekosStatusChanged(Ekos::CommunicationStatus status);
    void indiStatusChanged(Ekos::CommunicationStatus status);
    void newLog(const QString &text);
    void newModule(const QString &name);

  protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);
    void resizeEvent(QResizeEvent *);

  public slots:

    /**
     * DBUS interface function.
     * Connects all the INDI devices started by Ekos.
     */
    Q_SCRIPTABLE Q_NOREPLY void connectDevices();

    /**
     * DBUS interface function.
     * Disconnects all the INDI devices started by Ekos.
     */
    Q_SCRIPTABLE Q_NOREPLY void disconnectDevices();

    /** @}*/

    void processINDI();
    void cleanDevices(bool stopDrivers = true);

    void processNewDevice(ISD::GDInterface *);
    void processNewProperty(INDI::Property *);
    void processNewNumber(INumberVectorProperty *nvp);
    void processNewText(ITextVectorProperty *tvp);

  private slots:

    void changeAlwaysOnTop(Qt::ApplicationState state);

    void showEkosOptions();

    void updateLog();
    void clearLog();

    void processTabChange();

    void processServerTermination(const QString &host, const QString &port);

    void removeDevice(ISD::GDInterface *);

    void deviceConnected();
    void deviceDisconnected();

    //void processINDIModeChange();
    void checkINDITimeout();

    // Logs
    void updateDebugInterfaces();
    void watchDebugProperty(ISwitchVectorProperty *svp);

    void setTelescope(ISD::GDInterface *);
    void setCCD(ISD::GDInterface *);
    void setFilter(ISD::GDInterface *);
    void setFocuser(ISD::GDInterface *);
    void setDome(ISD::GDInterface *);
    void setWeather(ISD::GDInterface *);
    void setDustCap(ISD::GDInterface *);
    void setLightBox(ISD::GDInterface *);
    void setST4(ISD::ST4 *);

    // Profiles
    void addProfile();
    void editProfile();
    void deleteProfile();
    void wizardProfile();

    // Mount Summary
    void updateMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt);
    void updateMountStatus(ISD::Telescope::Status status);
    void setTarget(SkyObject *o);

    // Capture Summary
    void updateCaptureStatus(CaptureState status);
    void updateCaptureProgress(SequenceJob *job);
    void updateExposureProgress(SequenceJob *job);
    void updateCaptureCountDown();

    // Focus summary
    void setFocusStatus(FocusState status);
    void updateFocusStarPixmap(QPixmap &starPixmap);
    void updateFocusProfilePixmap(QPixmap &profilePixmap);
    void updateCurrentHFR(double newHFR, int position);

    // Guide Summary
    void updateGuideStatus(GuideState status);
    void updateGuideStarPixmap(QPixmap &starPix);
    void updateGuideProfilePixmap(QPixmap &profilePix);
    void updateSigmas(double ra, double de);

  private:
    void removeTabs();
    void reset();
    void initCapture();
    void initFocus();
    void initGuide();
    void initAlign();
    void initMount();
    void initDome();
    void initWeather();
    void initDustCap();

    void loadDrivers();
    void loadProfiles();

    // Connect Signals/Slots of Ekos modules
    void connectModules();

    // Check if INDI server is already running
    bool isRunning(const QString &process);

    // Find List of devices of specific family type
    QList<ISD::GDInterface *> findDevices(DeviceFamily type);

    ProfileInfo *getCurrentProfile();
    void getCurrentProfileTelescopeInfo(double &primaryFocalLength, double &primaryAperture, double &guideFocalLength, double &guideAperture);
    void updateProfileLocation(ProfileInfo *pi);

    bool useGuideHead { false };
    bool useST4 { false };

    // Containers

    // All Drivers
    QHash<QString, DriverInfo *> driversList;

    // All managed drivers
    QList<DriverInfo *> managedDrivers;

    // All generic devices (i.e. those define by INDI server)
    QList<ISD::GDInterface *> genericDevices;

    // All Managed devices (ie. those explicitly defined in the profile)
    QMap<DeviceFamily, ISD::GDInterface *> managedDevices;

    // Smart pointers for the various Ekos Modules
    std::unique_ptr<Capture> captureProcess;
    std::unique_ptr<Focus> focusProcess;
    std::unique_ptr<Guide> guideProcess;
    std::unique_ptr<Align> alignProcess;
    std::unique_ptr<Mount> mountProcess;
    std::unique_ptr<Scheduler> schedulerProcess;
    std::unique_ptr<Dome> domeProcess;
    std::unique_ptr<Weather> weatherProcess;
    std::unique_ptr<DustCap> dustCapProcess;
    std::unique_ptr<EkosLive::Client> ekosLiveClient;

    bool m_LocalMode { true };
    bool m_isStarted { false };
    bool m_RemoteManagerStart { false };

    int nDevices { 0 };

    QStringList m_LogText;
    KPageWidgetItem *ekosOptionsWidget { nullptr };

    CommunicationStatus m_ekosStatus { Ekos::Idle };
    CommunicationStatus m_indiStatus { Ekos::Idle };

    std::unique_ptr<QStandardItemModel> profileModel;
    QList<std::shared_ptr<ProfileInfo>> profiles;

    // Filter Manager
    QSharedPointer<FilterManager> filterManager;

    // Mount Summary
    QProgressIndicator *mountPI { nullptr };

    // Capture Summary
    QTime imageCountDown;
    QTime overallCountDown;
    QTime sequenceCountDown;
    QTimer countdownTimer;
    QProgressIndicator *capturePI { nullptr };
    // Preview Frame
    std::unique_ptr<FITSView> summaryPreview;

    // Focus Summary
    QProgressIndicator *focusPI { nullptr };
    std::unique_ptr<QPixmap> focusStarPixmap;

    // Guide Summary
    QProgressIndicator *guidePI { nullptr };
    std::unique_ptr<QPixmap> guideStarPixmap;

    ProfileInfo *currentProfile { nullptr };
    bool profileWizardLaunched { false };

    // Serial Port Assistant
    std::unique_ptr<SerialPortAssistant> serialPortAssistant;

    // Logs
    QPointer<OpsLogs> opsLogs;

    friend class EkosLive::Client;
    friend class EkosLive::Message;
    friend class EkosLive::Media;
};

}
