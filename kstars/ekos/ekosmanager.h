#ifndef EKOSMANAGER_H
#define EKOSMANAGER_H

/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include <baseclient.h>

#include "ui_ekosmanager.h"
#include "indi/indistd.h"
#include "capture.h"
#include "focus.h"
#include "guide.h"
#include "align.h"
#include "mount.h"

#include <QDialog>
#include <QHash>
#include <QtMultimedia/QMediaPlayer>
#include <QtDBus/QtDBus>

class DriverInfo;
class KPageWidgetItem;

/**
 *@class EkosManager
 *@short Primary class to handle all Ekos modules.
 * The Ekos Manager class manages startup and shutdown of INDI devices and registeration of devices within Ekos Modules. Ekos module consist of \ref Ekos::Mount, \ref Ekos::Capture, \ref Ekos::Focus, \ref Ekos::Guide, and \ref Ekos::Align modules.
 * \ref EkosDBusInterface "Ekos DBus Interface" provides high level functions to control devices and Ekos modules for a total robotic operation:
 * <ul>
 * <li>\ref CaptureDBusInterface "Capture Module DBus Interface"</li>
 * <li>\ref FocusDBusInterface "Focus Module DBus Interface"</li>
 * <li>\ref MountDBusInterface "Mount Module DBus Interface"</li>
 * <li>\ref GuideDBusInterface "Guide Module DBus Interface"</li>
 * <li>\ref AlignDBusInterface "Align Module DBus Interface"</li>
 * </ul>
 *  For low level access to INDI devices, the \ref INDIDBusInterface "INDI Dbus Interface" provides complete access to INDI devices and properties.
 *@author Jasem Mutlaq
 *@version 1.2
 */
class EkosManager : public QDialog, public Ui::EkosManager
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos")

public:
    EkosManager();
    ~EkosManager();

    typedef enum { STATUS_IDLE, STATUS_PENDING, STATUS_SUCCESS, STATUS_ERROR } CommunicationStatus;

    void appendLogText(const QString &);
    void refreshRemoteDrivers();
    void setOptionsWidget(KPageWidgetItem *ops) { ekosOption = ops; }

    /** @defgroup EkosDBusInterface Ekos DBus Interface
     * EkosManager interface provides advanced scripting capabilities to establish and shutdown Ekos services.
    */

    /*@{*/

    /** DBUS interface function.
     * set Ekos connection mode.
     * @param isLocal if true, it will establish INDI server locally, otherwise it will connect to a remote INDI server as defined in the Ekos options or by the user.
     * /note This function must be called before all functions in Ekos DBUS Interface.
     */
    Q_SCRIPTABLE Q_NOREPLY void setConnectionMode(bool isLocal);

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

    /** DBUS interface function.
     * Sets the telescope driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote telescope driver.
     * @param telescopeName telescope driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setTelescope(const QString & telescopeName);

    /** DBUS interface function.
     * Sets the CCD driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote CCD driver.
     * @param CCDName CCD driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setCCD(const QString & ccdName);

    /** DBUS interface function.
     * Sets the guider driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote guider driver.
     * @param guiderName guider CCD driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup. If the primary CCD has a guide chip,
     * do not set the guider name as the guide chip will be automatically selected as guider
     */
    Q_SCRIPTABLE Q_NOREPLY void setGuider(const QString & guiderName);

    /** DBUS interface function.
     * Sets the focuser driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote focuser driver.
     * @param focuserName focuser driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setFocuser(const QString & focuserName);

    /** DBUS interface function.
     * Sets the AO driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote AO driver.
     * @param AOName Adaptive Optics driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAO(const QString & AOName);

    /** DBUS interface function.
     * Sets the filter driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote filter driver.
     * @param filterName filter driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setFilter(const QString & filterName);

    /** DBUS interface function.
     * Sets the dome driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote dome driver.
     * @param domeName dome driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setDome(const QString & domeName);

    /** DBUS interface function.
     * Sets the auxiliary driver name. If connection mode is local, it is selected from the local drivers combo box. Otherwise, it is set as the remote auxiliary driver.
     * @param index 1 for Aux 1, 2 for Aux 2, 3 for Aux 3
     * @param auxiliaryName auxiliary driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAuxiliary(int index, const QString & auxiliaryName);    

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
    void cleanDevices();

    void processNewDevice(ISD::GDInterface*);
    void processNewProperty(INDI::Property*);
    void processNewNumber(INumberVectorProperty *nvp);
    void processNewText(ITextVectorProperty *tvp);

    Ekos::Capture *captureModule() { return captureProcess;}
    Ekos::Focus *focusModule() { return focusProcess;}
    Ekos::Guide *guideModule() { return guideProcess;}
    Ekos::Align *alignModule() { return alignProcess;}
    Ekos::Mount *mountModule() { return mountProcess;}

protected slots:

    void updateLog();
    void clearLog();

    void processTabChange();

    void removeDevice(ISD::GDInterface*);

    void deviceConnected();
    void deviceDisconnected();

    void processINDIModeChange();
    void checkINDITimeout();

    void setTelescope(ISD::GDInterface *);
    void setCCD(ISD::GDInterface *);
    void setFilter(ISD::GDInterface *);
    void setFocuser(ISD::GDInterface *);
    void setST4(ISD::ST4 *);    

 private:

    void removeTabs();
    void reset();
    void initCapture();
    void initFocus();
    void initGuide();
    void initAlign();
    void initMount();

    void initLocalDrivers();
    void initRemoteDrivers();

    void saveLocalDrivers();
    void saveRemoteDrivers();

    void processLocalDevice(ISD::GDInterface*);
    void processRemoteDevice(ISD::GDInterface*);
    bool isRunning(const QString &process);

    bool useGuideHead;
    bool useST4;
    bool guideStarted;
    bool ccdStarted;
    bool scopeRegistered;
    bool remoteCCDRegistered;
    bool remoteGuideRegistered;

    ISD::GDInterface *scope, *ccd, *guider, *focuser, *filter, *aux1, *aux2, *aux3, *dome, *ao;
    DriverInfo *scope_di, *ccd_di, *guider_di, *filter_di, *focuser_di, *aux1_di, *aux2_di, *aux3_di, *ao_di, *dome_di, *remote_indi;

    Ekos::Capture *captureProcess;
    Ekos::Focus *focusProcess;
    Ekos::Guide *guideProcess;
    Ekos::Align *alignProcess;
    Ekos::Mount *mountProcess;

    QString guiderCCDName;
    QString primaryCCDName;
    bool localMode, ccdDriverSelected;

    int nDevices;
    int nConnectedDevices;
    QList<DriverInfo *> managedDevices;
    QHash<QString, DriverInfo *> driversList;
    QStringList logText;
    KPageWidgetItem *ekosOption;
    CommunicationStatus ekosStartingStatus, indiConnectionStatus;

};


#endif // EKOS_H
