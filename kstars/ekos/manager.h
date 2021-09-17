/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifdef USE_QT5_INDI
#include <baseclientqt.h>
#else
#include <baseclient.h>
#endif

#include "ui_manager.h"

#include "ekos.h"
#include "manager/focusmanager.h"
#include "manager/guidemanager.h"
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
#include "analyze/analyze.h"
#include "observatory/observatory.h"
#include "auxiliary/filtermanager.h"
#include "auxiliary/portselector.h"
#include "ksnotification.h"
// Can't use forward declaration with QPointer. QTBUG-29588
#include "auxiliary/opslogs.h"

#include <QDialog>
#include <QHash>
#include <QtDBus/QtDBus>

#include <memory>

//! Generic record interfaces and implementations.
namespace EkosLive
{
class Client;
class Message;
class Media;
}

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
 * @version 1.8
 */
namespace Ekos
{

class Manager : public QDialog, public Ui::Manager
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos")

        Q_SCRIPTABLE Q_PROPERTY(Ekos::CommunicationStatus indiStatus READ indiStatus NOTIFY indiStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(Ekos::CommunicationStatus ekosStatus READ ekosStatus NOTIFY ekosStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(Ekos::CommunicationStatus settleStatus READ settleStatus NOTIFY settleStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(bool ekosLiveStatus READ ekosLiveStatus NOTIFY ekosLiveStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

        enum class EkosModule
        {
            Setup,
            Scheduler,
            Analyze,
            Capture,
            Focus,
            Mount,
            Align,
            Guide,
            Observatory,
        };
    public:
        static Manager *Instance();
        static void release();

        // No OP
        void initialize() {}

        void appendLogText(const QString &);
        void setOptionsWidget(KPageWidgetItem *ops)
        {
            ekosOptionsWidget = ops;
        }
        void addObjectToScheduler(SkyObject *object);

        Scheduler *schedulerModule()
        {
            return schedulerProcess.get();
        }
        Guide *guideModule()
        {
            return guideProcess.get();
        }
        Align *alignModule()
        {
            return alignProcess.get();
        }
        Mount *mountModule()
        {
            return mountProcess.get();
        }
        Focus *focusModule()
        {
            return focusProcess.get();
        }
        Dome  *domeModule()
        {
            return domeProcess.get();
        }
        DustCap *capModule()
        {
            return dustCapProcess.get();
        }
        Capture *captureModule()
        {
            return captureProcess.get();
        }
        FITSView *getSummaryPreview()
        {
            return summaryPreview.get();
        }
        FilterManager *getFilterManager()
        {
            return filterManager.data();
        }
        QString getCurrentJobName();
        void announceEvent(const QString &message, KSNotification::EventType event);

        /**
         * @brief addProfile Add a new profile to the database.
         * @param profileInfo Collection of profile parameters to include the following:
         * 1. name: Profile name
         * 2. auto_connect: True of False for Autoconnect?
         * 3. Mode: "local" or "remote"
         * 4. remote_host: Optional. remote host (default localhost)
         * 5. remote_port: Optional. remote port (default 7624)
         * 6. guiding: 0 for "Internal", 1 for "PHD2", or 2 for "LinGuider"
         * 7. remote_guiding_host: Optional. remote host for guider application (default localhost)
         * 8. remote_guide_port: Optional. remote port for guider application.
         * 9. use_web_manager: True or False?
         * 10. web_manager_port. Optional. INDI Web Manager port (default 8624)
         * 12. primary_scope: ID of primary scope to use. This is the ID from OAL::Scope list in the database.
         * 13. guide_scope: ID of guide scope to use. This is the ID from OAL::Scope list in the database.
         * 14. mount: Mount driver label (default --).
         * 15. ccd: CCD driver label (default --).
         * 16. guider: Guider driver label (default --).
         * 17. focuser: Focuser driver label (default --).
         * 18. filter: Filter Wheel driver label (default --).
         * 19. ao: Adaptive Optics driver label (default --).
         * 20. dome: Dome driver label (default --).
         * 21. Weather: Weather station driver label (default --).
         * 22. aux1: aux1 driver label (default --).
         * 23. aux2: aux2 driver label (default --).
         * 24. aux3: aux3 driver label (default --).
         * 25. aux4: aux4 driver label (default --).
         */
        void addNamedProfile(const QJsonObject &profileInfo);

        /** Same as above, except it edits an existing named profile */
        void editNamedProfile(const QJsonObject &profileInfo);

        /**
         * @brief deleteProfile Delete existing equipment profile
         * @param name Name of profile
         * @warning Ekos must be stopped for this to work. It will fail if Ekos is online.
         */
        void deleteNamedProfile(const QString &name);

        /**
         * @brief getProfile Get a single profile information.
         * @param name Profile name
         * @return A JSon object with the detail profile info as described in addProfile function.
         */
        QJsonObject getNamedProfile(const QString &name);

        /**
         * DBus commands to manage equipment profiles.
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

        /** @}*/


        /**
         * Manager interface provides advanced scripting capabilities to establish and shutdown Ekos services.
         */

        /*@{*/

        /**
         * DBUS interface function.
         * @return INDI connection status (0 Idle, 1 Pending, 2 Connected, 3 Error)
         * @deprecated
         */
        Q_SCRIPTABLE unsigned int getINDIConnectionStatus()
        {
            return m_indiStatus;
        }

        Q_SCRIPTABLE Ekos::CommunicationStatus indiStatus()
        {
            return m_indiStatus;
        }

        /**
         * DBUS interface function.
         * @return Ekos starting status (0 Idle, 1 Pending, 2 Started, 3 Error)
         * @deprecated
         */
        Q_SCRIPTABLE unsigned int getEkosStartingStatus()
        {
            return m_ekosStatus;
        }

        Q_SCRIPTABLE Ekos::CommunicationStatus ekosStatus()
        {
            return m_ekosStatus;
        }

        /**
         * DBUS interface function.
         * @return Settle status (0 Idle, 1 Pending, 2 Started, 3 Error)
         */
        Q_SCRIPTABLE Ekos::CommunicationStatus settleStatus()
        {
            return m_settleStatus;
        }

        /**
         * DBUS interface function. Toggle Ekos logging.
         * @param name Name of logging to toggle. Available options are:
         * ** VERBOSE
         * ** INDI
         * ** FITS
         * ** CAPTURE
         * ** FOCUS
         * ** GUIDE
         * ** ALIGNMENT
         * ** MOUNT
         * ** SCHEDULER
         * ** OBSERVATORY
         * @param enabled True to enable, false otherwise.
         */
        Q_SCRIPTABLE Q_NOREPLY void setEkosLoggingEnabled(const QString &name, bool enabled);

        /**
         * DBUS interface function.
         * If connection mode is local, the function first establishes an INDI server with all the specified drivers in Ekos options or as set by the user. For remote connection,
         * it establishes connection to the remote INDI server.
         * @return Returns true if server started successful (local mode) or connection to remote server is successful (remote mode).
         */
        Q_SCRIPTABLE void start();

        /**
         * DBUS interface function.
         * If connection mode is local, the function terminates the local INDI server and drivers. For remote, it disconnects from the remote INDI server.
         */
        Q_SCRIPTABLE void stop();

        Q_SCRIPTABLE QStringList logText()
        {
            return m_LogText;
        }

        Q_SCRIPTABLE bool ekosLiveStatus();

        /**
         * DBUS interface function.
         * @param enabled Connect to EkosLive if true, otherwise disconnect.
        */
        Q_SCRIPTABLE void setEkosLiveConnected(bool enabled);

        /**
         * @brief setEkosLiveConfig Set EkosLive settings
         * @param onlineService If true, connect to EkosLive Online Service. Otherwise, EkosLive offline service.
         * @param rememberCredentials Remember username and password for next session.
         * @param autoConnect If true, it will automatically connect to EkosLive service.
         */
        Q_SCRIPTABLE void setEkosLiveConfig(bool onlineService, bool rememberCredentials, bool autoConnect);

        /**
         * @brief setEkosLiveUser Save EkosLive username and password
         * @param username User name
         * @param password Password
         */
        Q_SCRIPTABLE void setEkosLiveUser(const QString &username, const QString &password);

        /**
         * @brief acceptPortSelection Accept current port selection settings in the Selector Dialog
         */
        Q_SCRIPTABLE void acceptPortSelection();

    signals:
        // Have to use full Ekos::CommunicationStatus for DBus signal to work
        void ekosStatusChanged(Ekos::CommunicationStatus status);
        void indiStatusChanged(Ekos::CommunicationStatus status);
        void settleStatusChanged(Ekos::CommunicationStatus status);
        void ekosLiveStatusChanged(bool status);

        void newLog(const QString &text);
        void newModule(const QString &name);

    protected:
        void closeEvent(QCloseEvent *event) override;
        void hideEvent(QHideEvent *) override;
        void showEvent(QShowEvent *) override;
        void resizeEvent(QResizeEvent *) override;

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
        void processNewProperty(INDI::Property);
        void processDeleteProperty(const QString &name);

        void processNewNumber(INumberVectorProperty *nvp);
        void processNewText(ITextVectorProperty *tvp);
        void processNewSwitch(ISwitchVectorProperty *svp);
        void processNewLight(ILightVectorProperty *lvp);
        void processNewBLOB(IBLOB *bvp);

        void restartDriver(const QString &deviceName);

    private slots:

        void changeAlwaysOnTop(Qt::ApplicationState state);

        void showEkosOptions();

        void updateLog();
        void clearLog();

        void processTabChange();

        void processServerTerminated(const QString &host, const QString &port);
        void processServerStarted(const QString &host, const QString &port);

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
        void updateMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt, int pierSide,
                               const QString &ha);
        void updateMountStatus(ISD::Telescope::Status status);
        void setTarget(SkyObject *o);

        // Capture Summary
        void updateCaptureStatus(CaptureState status);
        void updateCaptureProgress(SequenceJob *job, const QSharedPointer<FITSData> &data);
        void updateExposureProgress(SequenceJob *job);
        void updateCaptureCountDown();

        // Focus summary
        void updateFocusStatus(FocusState status);
        void updateCurrentHFR(double newHFR, int position);
        const QString getFocusStatusText()
        {
            return focusManager->focusStatus->text();
        }

        // Guide Summary
        void updateGuideStatus(GuideState status);
        void updateSigmas(double ra, double de);
        const QString getGuideStatusText()
        {
            return guideManager->guideStatus->text();
        }

    private:
        explicit Manager(QWidget *parent);
        ~Manager() override;

        void removeTabs();
        void reset();
        void initCapture();
        void initFocus();
        void initGuide();
        void initAlign();
        void initMount();
        void initDome();
        void initWeather();
        void initObservatory(Weather *weather, Dome *dome);
        void initDustCap();

        void loadDrivers();
        void loadProfiles();
        int addModuleTab(EkosModule module, QWidget *tab, const QIcon &icon);

        /**
         * @brief syncActiveDevices Syncs ACTIVE_DEVICES such as ACTIVE_TELESCOPE and ACTIVE_CCD
         * to the currently detected devices.
         */
        void syncActiveDevices();

        // Connect Signals/Slots of Ekos modules
        void connectModules();

        // Check if INDI server is already running
        bool isRunning(const QString &process);

        // Find List of devices of specific family type
        QList<ISD::GDInterface *> findDevices(DeviceFamily type);
        // Find list of devices by device interface
        QList<ISD::GDInterface *> findDevicesByInterface(uint32_t interface);
        // Get all detected devices
        const QList<ISD::GDInterface *> &getAllDevices() const;

        ProfileInfo *getCurrentProfile();
        void getCurrentProfileTelescopeInfo(double &primaryFocalLength, double &primaryAperture, double &guideFocalLength,
                                            double &guideAperture);
        void updateProfileLocation(ProfileInfo *pi);
        void setProfileMapping(const QJsonObject &payload)
        {
            m_ProfileMapping = payload;
        }
        // Port Selector Save profile when connect all is pressed
        void setPortSelectionComplete();
        // Check if the driver binary must be one only to avoid duplicate instances
        // Some driver binaries support multiple devices per binary
        // so we only need to start a single instance to handle them all.
        bool checkUniqueBinaryDriver(DriverInfo * primaryDriver, DriverInfo * secondaryDriver);

        bool useGuideHead { false };
        bool useST4 { false };

        // Containers

        // All Drivers
        QHash<QString, DriverInfo *> driversList;

        // All managed drivers
        QList<DriverInfo *> managedDrivers;

        // All generic devices (i.e. those define by INDI server)
        QList<ISD::GDInterface *> genericDevices;

        // All proxy devices (generated devices by Ekos Manager for specific interfaces)
        QList<ISD::GDInterface *> proxyDevices;

        // All Managed devices (ie. those explicitly defined in the profile)
        QMap<DeviceFamily, ISD::GDInterface *> managedDevices;

        // Smart pointers for the various Ekos Modules
        std::unique_ptr<Capture> captureProcess;
        std::unique_ptr<Focus> focusProcess;
        std::unique_ptr<Guide> guideProcess;
        std::unique_ptr<Align> alignProcess;
        std::unique_ptr<Mount> mountProcess;
        std::unique_ptr<Analyze> analyzeProcess;
        std::unique_ptr<Scheduler> schedulerProcess;
        std::unique_ptr<Observatory> observatoryProcess;
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
        // Settle is used to know once all properties from all devices have been defined
        // There is no way to know this for sure so we use a debounace mechanism.
        CommunicationStatus m_settleStatus { Ekos::Idle };

        std::unique_ptr<QStandardItemModel> profileModel;
        QList<std::shared_ptr<ProfileInfo>> profiles;
        QJsonObject m_ProfileMapping;

        // Filter Manager
        QSharedPointer<FilterManager> filterManager;

        // Mount Summary
        QPointer<QProcess> indiHubAgent;

        // Capture Summary
        QTimer countdownTimer;
        QTimer settleTimer;
        // Preview Frame
        std::unique_ptr<FITSView> summaryPreview;

        ProfileInfo *currentProfile { nullptr };
        bool profileWizardLaunched { false };

        // Port Selector
        std::unique_ptr<Selector::Dialog> m_PortSelector;
        QTimer m_PortSelectorTimer;

        // Logs
        QPointer<OpsLogs> opsLogs;

        // E.g. Setup, Scheduler, and Analyze.
        int numPermanentTabs { 0 };

        friend class EkosLive::Client;
        friend class EkosLive::Message;
        friend class EkosLive::Media;

        static Manager *_Manager;
};

}
