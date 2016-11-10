#ifndef EKOSMANAGER_H
#define EKOSMANAGER_H

/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#ifdef USE_QT5_INDI
#include <baseclientqt.h>
#else
#include <baseclient.h>
#endif

#include "ui_ekosmanager.h"

#include "ekos.h"
#include "indi/indistd.h"
#include "capture/capture.h"
#include "focus/focus.h"
#include "guide/guide.h"
#include "align/align.h"
#include "mount/mount.h"
#include "auxiliary/dome.h"
#include "auxiliary/weather.h"
#include "auxiliary/dustcap.h"
#include "scheduler/scheduler.h"

#include <QDialog>
#include <QHash>
#include <QtDBus/QtDBus>

class DriverInfo;
class ProfileInfo;
class KPageWidgetItem;
class QProgressIndicator;

/**
 *@class Manager
 *@short Primary class to handle all Ekos modules.
 * The Ekos Manager class manages startup and shutdown of INDI devices and registeration of devices within Ekos Modules. Ekos module consist of \ref Ekos::Mount, \ref Ekos::Capture, \ref Ekos::Focus, \ref Ekos::Guide, and \ref Ekos::Align modules.
 * \ref EkosDBusInterface "Ekos DBus Interface" provides high level functions to control devices and Ekos modules for a total robotic operation:
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
 *@author Jasem Mutlaq
 *@version 1.5
 */
class EkosManager : public QDialog, public Ui::EkosManager
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos")

public:
    EkosManager(QWidget *parent);
    ~EkosManager();

    typedef enum { EKOS_STATUS_IDLE, EKOS_STATUS_PENDING, EKOS_STATUS_SUCCESS, EKOS_STATUS_ERROR } CommunicationStatus;

    void appendLogText(const QString &);
    //void refreshRemoteDrivers();
    void setOptionsWidget(KPageWidgetItem *ops) { ekosOption = ops; }
    void addObjectToScheduler(SkyObject *object);

    Ekos::Capture *captureModule() { return captureProcess;}
    Ekos::Focus *focusModule() { return focusProcess;}
    Ekos::Guide *guideModule() { return guideProcess;}
    Ekos::Align *alignModule() { return alignProcess;}
    Ekos::Mount *mountModule() { return mountProcess;}

    /** @defgroup EkosDBusInterface Ekos DBus Interface
     * EkosManager interface provides advanced scripting capabilities to establish and shutdown Ekos services.
    */

    /*@{*/

    /** DBUS interface function.
     * set Current device profile.
     * @param profileName Profile name
     * @return True if profile is set, false if not found.
     */
    Q_SCRIPTABLE bool setProfile(const QString &profileName);

    /** DBUS interface function
     * @brief getProfiles Return a list of all device profiles
     * @return List of device profiles
     */
    Q_SCRIPTABLE QStringList getProfiles();

    /** DBUS interface function.
     * @retrun INDI connection status (0 Idle, 1 Pending, 2 Connected, 3 Error)
     */
    Q_SCRIPTABLE unsigned int getINDIConnectionStatus() { return indiConnectionStatus;}

    /** DBUS interface function.
     * @retrun Ekos starting status (0 Idle, 1 Pending, 2 Started, 3 Error)
     */
    Q_SCRIPTABLE unsigned int getEkosStartingStatus() { return ekosStartingStatus;}

    /** DBUS interface function.
     * If connection mode is local, the function first establishes an INDI server with all the specified drivers in Ekos options or as set by the user. For remote connection,
     * it establishes connection to the remote INDI server.
     * @return Retruns true if server started successful (local mode) or connection to remote server is successful (remote mode).
     */
    Q_SCRIPTABLE bool start();

    /** DBUS interface function.
     * If connection mode is local, the function terminates the local INDI server and drivers. For remote, it disconnects from the remote INDI server.
     */
    Q_SCRIPTABLE bool stop();

protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);
    void resizeEvent(QResizeEvent *);

public slots:

    /** DBUS interface function.
     * Connects all the INDI devices started by Ekos.
     */
    Q_SCRIPTABLE Q_NOREPLY void connectDevices();

    /** DBUS interface function.
     * Disconnects all the INDI devices started by Ekos.
     */
    Q_SCRIPTABLE Q_NOREPLY void disconnectDevices();

    /** @}*/

    void processINDI();
    void cleanDevices(bool stopDrivers=true);

    void processNewDevice(ISD::GDInterface*);
    void processNewProperty(INDI::Property*);
    void processNewNumber(INumberVectorProperty *nvp);
    void processNewText(ITextVectorProperty *tvp);

private slots:

    void changeAlwaysOnTop(Qt::ApplicationState state);

    void updateLog();
    void clearLog();

    void processTabChange();

    void processServerTermination(const QString &host, const QString &port);

    void removeDevice(ISD::GDInterface*);

    void deviceConnected();
    void deviceDisconnected();

    //void processINDIModeChange();
    void checkINDITimeout();

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
    void saveDefaultProfile(const QString& name);

    // Mount Summary
    void updateMountCoords(const QString &ra, const QString &dec ,const QString &az ,const QString &alt);
    void updateMountStatus(ISD::Telescope::TelescopeStatus status);
    void setTarget(SkyObject *o);

    // Capture Summary
    void updateCaptureStatus(Ekos::CaptureState status);
    void updateCaptureProgress(QImage *image, Ekos::SequenceJob *job);
    void updateExposureProgress(Ekos::SequenceJob *job);
    void updateCaptureCountDown();

    // Focus summary
    void setFocusStatus(Ekos::FocusState status);
    void updateFocusStarPixmap(QPixmap &starPixmap);
    void updateFocusProfilePixmap(QPixmap &profilePixmap);

    // Guide Summary
    void updateGuideStatus(Ekos::GuideState status);
    void updateGuideStarPixmap(QPixmap &starPix);
    void updateGuideProfilePixmap(QPixmap &profilePix);

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

    void processLocalDevice(ISD::GDInterface*);
    void processRemoteDevice(ISD::GDInterface*);
    bool isRunning(const QString &process);

    bool useGuideHead;
    bool useST4;

    // Containers

    // All Drivers
    QHash<QString, DriverInfo *> driversList;

    // All managed drivers
    QList<DriverInfo *> managedDrivers;

    // All generic devices
    QList<ISD::GDInterface*> genericDevices;

    // All Managed devices
    QMap<DeviceFamily, ISD::GDInterface*> managedDevices;
    QList<ISD::GDInterface*> findDevices(DeviceFamily type);

    Ekos::Capture *captureProcess;
    Ekos::Focus *focusProcess;
    Ekos::Guide *guideProcess;
    Ekos::Align *alignProcess;
    Ekos::Mount *mountProcess;
    Ekos::Scheduler *schedulerProcess;
    Ekos::Dome *domeProcess;
    Ekos::Weather *weatherProcess;
    Ekos::DustCap *dustCapProcess;

    bool localMode, isStarted, remoteManagerStart;

    int nDevices, nRemoteDevices;
    //QAtomicInt nConnectedDevices;

    QStringList logText;
    KPageWidgetItem *ekosOption;
    CommunicationStatus ekosStartingStatus, indiConnectionStatus;

    QStandardItemModel *profileModel;
    QList<ProfileInfo *> profiles;

    ProfileInfo * getCurrentProfile();
    void updateProfileLocation(ProfileInfo *pi);

    // Mount Summary
    QProgressIndicator *mountPI;

    // Capture Summary
    QTime imageCountDown;
    QTime overallCountDown;
    QTime sequenceCountDown;
    QTimer countdownTimer;
    QPixmap *previewPixmap;
    QProgressIndicator *capturePI;

    // Focus Summary
    QProgressIndicator *focusPI;
    QPixmap *focusStarPixmap;
    //QPixmap *focusProfilePixmap;
    //QTemporaryFile focusStarFile;
    //QTemporaryFile focusProfileFile;

    // Guide Summary
    QProgressIndicator *guidePI;
    QPixmap *guideStarPixmap;
    //QPixmap *guideProfilePixmap;
    //QTemporaryFile guideStarFile;
    //QTemporaryFile guideProfileFile;

};

#endif // EKOS_H
