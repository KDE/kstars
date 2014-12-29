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

#include <QDialog>
#include <QHash>
#include <QtMultimedia/QMediaPlayer>
#include <QtDBus/QtDBus>

class DriverInfo;

/**
 *@class EkosManager
 *@short Primary class to handle all Ekos modules.
 * The Ekos Manager class manages startup and shutdown of INDI devices and registeration of devices within Ekos Modules. Ekos module consist of Capture, Focus, Guide, and Align modules.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class EkosManager : public QDialog, public Ui::EkosManager
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos")

public:
    EkosManager(QWidget *parent);
    ~EkosManager();

    void appendLogText(const QString &);
    void refreshRemoteDrivers();

    void playFITS();
    void playOk();
    void playError();

    /** DBUS interface function.
     * set Ekos connection mode.
     * @param isLocal if true, it will establish INDI server locally, otherwise it will connect to a remote INDI server as defined in the Ekos options or by the user.
     * /note This function must be called before all functions in Ekos DBUS Interface.
     */
    Q_SCRIPTABLE Q_NOREPLY void setConnectionMode(bool isLocal);

    /** DBUS interface function.
     * @retrun Retruns true if all devices are conncted, false otherwise.
     */
    Q_SCRIPTABLE bool isConnected() { return disconnectB->isEnabled();}

    /** DBUS interface function.
     * @retrun Retruns true if all INDI drivers are started, false otherwise.
     */
    Q_SCRIPTABLE bool isStarted() { return controlPanelB->isEnabled();}

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
     * @param auxiliaryName auxiliary driver name. For remote devices, the name has to be exactly as the name defined by the driver on startup.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAuxiliary(const QString & auxiliaryName);

public slots:
    void processINDI();

    /** DBUS interface function.
     * Connects all the INDI devices started by Ekos.
     */
    Q_SCRIPTABLE Q_NOREPLY void connectDevices();

    /** DBUS interface function.
     * Disconnects all the INDI devices started by Ekos.
     */
    Q_SCRIPTABLE Q_NOREPLY void disconnectDevices();
    void cleanDevices();

    void processNewDevice(ISD::GDInterface*);
    void processNewProperty(INDI::Property*);
    void processNewNumber(INumberVectorProperty *nvp);
    void processNewText(ITextVectorProperty *tvp);

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

    void loadDefaultDrivers();
    void saveDefaultDrivers();
    void removeTabs();
    void reset();
    void initCapture();
    void initFocus();
    void initGuide();
    void initAlign();

    void initLocalDrivers();
    void initRemoteDrivers();

    void processLocalDevice(ISD::GDInterface*);
    void processRemoteDevice(ISD::GDInterface*);
    bool isRunning(const QString &process);

    bool useGuideHead;
    bool useST4;
    bool guideStarted;
    bool ccdStarted;
    bool scopeRegistered;

    ISD::GDInterface *scope, *ccd, *guider, *focuser, *filter, *aux, *dome, *ao;
    DriverInfo *scope_di, *ccd_di, *guider_di, *filter_di, *focuser_di, *aux_di, *ao_di, *dome_di, *remote_indi;

    Ekos::Capture *captureProcess;
    Ekos::Focus *focusProcess;
    Ekos::Guide *guideProcess;
    Ekos::Align *alignProcess;

    QString guiderName;
    bool localMode, ccdDriverSelected;

    unsigned short nDevices;
    QList<DriverInfo *> managedDevices;
    QHash<QString, DriverInfo *> driversList;
    QStringList logText;

    QMediaPlayer *playFITSFile, *playOkFile, *playErrorFile;

};


#endif // EKOS_H
