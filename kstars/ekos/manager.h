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
#include "fitsviewer/summaryfitsview.h"
#include "indi/indistd.h"
#include "auxiliary/portselector.h"
#include "ksnotification.h"
#include "auxiliary/opslogs.h"
#include "ekos/capture/rotatorsettings.h"
#include "extensions.h"

#include <QDialog>
#include <QHash>
#include <QTimer>

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
class OpsEkos;

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

class Analyze;
class Capture;
class Scheduler;
class Focus;
class FocusModule;
class Align;
class Guide;
class Mount;
class Observatory;
// class RotatorSettings;

class Manager : public QDialog, public Ui::Manager
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos")

        Q_SCRIPTABLE Q_PROPERTY(Ekos::CommunicationStatus indiStatus READ indiStatus NOTIFY indiStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(Ekos::CommunicationStatus ekosStatus READ ekosStatus NOTIFY ekosStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(Ekos::CommunicationStatus settleStatus READ settleStatus NOTIFY settleStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(bool ekosLiveStatus READ ekosLiveStatus NOTIFY ekosLiveStatusChanged)
        Q_SCRIPTABLE Q_PROPERTY(Ekos::ExtensionState extensionStatus READ extensionStatus NOTIFY extensionStatusChanged)
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
        void setOptionsWidget(KPageWidgetItem *ops, OpsEkos *opsEkosPtr)
        {
            ekosOptionsWidget = ops;
            opsEkos = opsEkosPtr;
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
        FocusModule *focusModule()
        {
            return focusProcess.get();
        }
        Capture *captureModule()
        {
            return captureProcess.get();
        }
        FITSView *getSummaryPreview()
        {
            return m_SummaryView.get();
        }

        // Filter Manager
        void createFilterManager(ISD::FilterWheel *device);
        bool getFilterManager(const QString &name, QSharedPointer<FilterManager> &fm);
        bool getFilterManager(QSharedPointer<FilterManager> &fm);

        // Rotator Control
        void createRotatorController(ISD::Rotator *device);
        bool getRotatorController(const QString &Name, QSharedPointer<RotatorSettings> &rs);
        bool existRotatorController();

        QString getCurrentJobName();
        void announceEvent(const QString &message, KSNotification::EventSource source, KSNotification::EventType event);

        extensions m_extensions;

        /**
         * @brief activateModule Switch tab to specific module name (i.e. CCD) and raise Ekos screen to focus.
         * @param name module name CCD, Guide, Focus, Mount, Scheduler, or Observatory.
         * @param popup if True, show Ekos Manager window in the foreground.
         */
        void activateModule(const QString &name, bool popup = false);

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
         * @return Settle status (0 EXTENSION_START_REQUESTED, 1 EXTENSION_STARTED, 2 EXTENSION_STOP_REQUESTED, 3 EXTENSION_STOPPED)
         */
        Q_SCRIPTABLE Ekos::ExtensionState extensionStatus()
        {
            return m_extensionStatus;
        };

        /**
         * DBUS interface function.
         * @param enabled Connect to EkosLive if true, otherwise disconnect.
        */
        Q_SCRIPTABLE void setEkosLiveConnected(bool enabled);

        /**
         * @brief setEkosLiveConfig Set EkosLive settings
         * @param rememberCredentials Remember username and password for next session.
         * @param autoConnect If true, it will automatically connect to EkosLive service.
         */
        Q_SCRIPTABLE void setEkosLiveConfig(bool rememberCredentials, bool autoConnect);

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
        void extensionStatusChanged(bool = true);

        void newLog(const QString &text);
        void newModule(const QString &name);
        void newDevice(const QString &name, int interface);

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

        /**
         * DBUS interface function.
         * Enables loading of preview fits from file rather than Capture data.
         */
        Q_SCRIPTABLE Q_NOREPLY void setFITSfromFile(bool previewFromFile);

        /**
         * DBUS interface function.
         * Load preview fits from file.
         */
        Q_SCRIPTABLE Q_NOREPLY void previewFile(QString filePath);

        /** @}*/

        void processINDI();
        void cleanDevices(bool stopDrivers = true);

        void processNewDevice(const QSharedPointer<ISD::GenericDevice> &device);

        void processNewProperty(INDI::Property);
        void processUpdateProperty(INDI::Property);
        void processDeleteProperty(INDI::Property);
        void processMessage(int id);


        void setDeviceReady();

        void restartDriver(const QString &deviceName);

    private slots:

        void changeAlwaysOnTop(Qt::ApplicationState state);

        void showEkosOptions();

        void updateLog();
        void clearLog();

        void processTabChange();

        void setServerStarted(const QString &host, int port);
        void setServerFailed(const QString &host, int port, const QString &message);
        //void setServerTerminated(const QString &host, int port, const QString &message);

        void setClientStarted(const QString &host, int port);
        void setClientFailed(const QString &host, int port, const QString &message);
        void setClientTerminated(const QString &host, int port, const QString &message);

        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);

        void deviceConnected();
        void deviceDisconnected();

        //void processINDIModeChange();
        void checkINDITimeout();

        // Logs
        void updateDebugInterfaces();
        void watchDebugProperty(INDI::Property prop);

        void addMount(ISD::Mount *device);
        void addCamera(ISD::Camera *device);
        void addFilterWheel(ISD::FilterWheel *device);
        void addFocuser(ISD::Focuser *device);
        void addRotator(ISD::Rotator *device);
        void addDome(ISD::Dome *device);
        void addWeather(ISD::Weather *device);
        void addDustCap(ISD::DustCap *device);
        void addLightBox(ISD::LightBox *device);
        void addGuider(ISD::Guider *device);
        void addGPS(ISD::GPS *device);

        /**
         * @brief syncGenericDevice Check if this device needs to be added to any Ekos module.
         * @param device pointer to generic device.
         */
        void syncGenericDevice(const QSharedPointer<ISD::GenericDevice> &device);
        void createModules(const QSharedPointer<ISD::GenericDevice> &device);

        // Profiles
        void addProfile();
        void editProfile();
        void deleteProfile();
        void wizardProfile();

        // Mount Summary
        void updateMountCoords(const SkyPoint position, ISD::Mount::PierSide pierSide, const dms &ha);
        void updateMountStatus(ISD::Mount::Status status);
        void setTarget(const QString &name);

        // Capture Summary
        void updateCaptureStatus(CaptureState status, const QString &trainname);
        void updateCaptureProgress(const QSharedPointer<SequenceJob> &job, const QSharedPointer<FITSData> &data, const QString &devicename = "");
        void updateExposureProgress(const QSharedPointer<SequenceJob> &job, const QString &trainname);
        void updateCaptureCountDown();

        // Focus summary
        void updateFocusStatus(FocusState status);
        void updateCurrentHFR(double newHFR, int position, bool inAutofocus);

        // Guide Summary
        void updateGuideStatus(GuideState status);
        void updateSigmas(double ra, double de);

        void help();

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
        void initObservatory();

        void loadDrivers();
        void loadProfiles();
        int addModuleTab(EkosModule module, QWidget *tab, const QIcon &icon);

        /**
         * @brief isINDIReady Check whether all INDI devices are connected and ready and emit signals accordingly.
         * @return True if all INDI devices are connected and ready.
         */
        bool isINDIReady();

        // Connect Signals/Slots of Ekos modules
        void connectModules();

        // Check if INDI server is already running
        bool isRunning(const QString &process);

        bool getCurrentProfile(QSharedPointer<ProfileInfo> &profile) const;
        void updateProfileLocation(const QSharedPointer<ProfileInfo> &profile);
        void setProfileMapping(const QJsonObject &payload)
        {
            m_ProfileMapping = payload;
        }
        // Port Selector Save profile when connect all is pressed
        void setPortSelectionComplete();
        // Check if the driver binary must be one only to avoid duplicate instances
        // Some driver binaries support multiple devices per binary
        // so we only need to start a single instance to handle them all.
        bool checkUniqueBinaryDriver(const QSharedPointer<DriverInfo> &primaryDriver,
                                     const QSharedPointer<DriverInfo> &secondaryDriver);

        // Containers

        // All Drivers
        QHash<QString, QSharedPointer<DriverInfo>> driversList;

        // All managed drivers
        QList<QSharedPointer<DriverInfo>> managedDrivers;

        // Smart pointers for the various Ekos Modules
        std::unique_ptr<Capture> captureProcess;
        std::unique_ptr<FocusModule> focusProcess;
        std::unique_ptr<Guide> guideProcess;
        std::unique_ptr<Align> alignProcess;
        std::unique_ptr<Mount> mountProcess;
        std::unique_ptr<Analyze> analyzeProcess;
        std::unique_ptr<Scheduler> schedulerProcess;
        std::unique_ptr<Observatory> observatoryProcess;
        std::unique_ptr<EkosLive::Client> ekosLiveClient;

        bool m_LocalMode { true };
        bool m_isStarted { false };
        bool m_RemoteManagerStart { false };

        int m_DriverDevicesCount { 0 };

        QStringList m_LogText;
        KPageWidgetItem *ekosOptionsWidget { nullptr };
        OpsEkos *opsEkos { nullptr };

        CommunicationStatus m_ekosStatus { Ekos::Idle };
        CommunicationStatus m_indiStatus { Ekos::Idle };
        // Settle is used to know once all properties from all devices have been defined
        // There is no way to know this for sure so we use a debounace mechanism.
        CommunicationStatus m_settleStatus { Ekos::Idle };
        ExtensionState m_extensionStatus { Ekos::EXTENSION_STOPPED };

        std::unique_ptr<QStandardItemModel> profileModel;
        QList<QSharedPointer<ProfileInfo>> profiles;
        QJsonObject m_ProfileMapping;

        // Mount Summary
        QPointer<QProcess> indiHubAgent;
        KLed *mountMotionState { nullptr };


        // Capture Summary
        QTimer m_CountdownTimer;
        QTimer settleTimer;
        // Preview Frame
        QSharedPointer<SummaryFITSView> m_SummaryView;
        bool FITSfromFile = false;
        QTimer extensionTimer;
        bool extensionAbort = false;

        QSharedPointer<ProfileInfo> m_CurrentProfile;
        bool profileWizardLaunched { false };
        QString m_PrimaryCamera, m_GuideCamera;

        // Port Selector
        std::unique_ptr<Selector::Dialog> m_PortSelector;
        QTimer m_PortSelectorTimer;

        QMap<QString, QSharedPointer<FilterManager>> m_FilterManagers;
        QMap<QString, QSharedPointer<RotatorSettings>> m_RotatorControllers;

        // Logs
        QPointer<OpsLogs> opsLogs;

        // E.g. Setup, Scheduler, and Analyze.
        int numPermanentTabs { 0 };

        // Used by the help button.
        bool checkIfPageExists(const QString &urlString);
        QNetworkAccessManager m_networkManager;

        friend class EkosLive::Client;
        friend class EkosLive::Message;
        friend class EkosLive::Media;

        static Manager *_Manager;
};

}
