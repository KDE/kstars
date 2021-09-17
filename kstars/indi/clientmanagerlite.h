/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "baseclientqt.h"
#include "indicommon.h"
#include "indistd.h"

#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <memory>

class QFileDialog;
class QQmlContext;

using namespace INDI;
class TelescopeLite;

struct DeviceInfoLite
{
    explicit DeviceInfoLite(INDI::BaseDevice *dev);
    ~DeviceInfoLite();

    INDI::BaseDevice *device { nullptr };
    /// Motion control
    std::unique_ptr<TelescopeLite> telescope;
};

/**
 * @class ClientManagerLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class ClientManagerLite : public INDI::BaseClientQt
{
        Q_OBJECT
        Q_PROPERTY(QString connectedHost READ connectedHost WRITE setConnectedHost NOTIFY connectedHostChanged)
        Q_PROPERTY(bool connected READ isConnected WRITE setConnected NOTIFY connectedChanged)

        /**
         * A wrapper for Options::lastServer(). Used to store last used server if user successfully
         * connected to some server at least once.
         **/
        Q_PROPERTY(QString lastUsedServer READ getLastUsedServer WRITE setLastUsedServer NOTIFY lastUsedServerChanged)

        /**
         * A wrapper for Options::lastServer(). Used to store last used port if user successfully
         * connected to some server at least once.
         **/
        Q_PROPERTY(int lastUsedPort READ getLastUsedPort WRITE setLastUsedPort NOTIFY lastUsedPortChanged)

        /**
         * A wrapper for Options::lastServer(). Used to store last Web Manager used port if user successfully
         * connected at least once.
         **/
        Q_PROPERTY(int lastUsedWebManagerPort READ getLastUsedWebManagerPort WRITE setLastUsedWebManagerPort
                   NOTIFY lastUsedWebManagerPortChanged)
    public:
        typedef enum { UPLOAD_CLIENT, UPLOAD_LOCAL, UPLOAD_BOTH } UploadMode;

        explicit ClientManagerLite(QQmlContext &main_context);
        virtual ~ClientManagerLite();

        Q_INVOKABLE bool setHost(const QString &ip, unsigned int port);
        Q_INVOKABLE void disconnectHost();

        /**
         * Get the profiles from Web Manager
         *
         * @param ip IP address
         * @param port Port number
         *
         * The process is async and the results are stored in @a webMProfiles. Once this
         * request finishes, the server status is queried from the server.
         */
        Q_INVOKABLE void getWebManagerProfiles(const QString &ip, unsigned int port);

        /**
         * Start an INDI server with a Web Manager profile
         *
         * @param profile Profile name
         */
        Q_INVOKABLE void startWebManagerProfile(const QString &profile);

        /** Stop the INDI server with an active Web Manager profile */
        Q_INVOKABLE void stopWebManagerProfile();

        /** Handle the errors of the async Web Manager requests */
        Q_INVOKABLE void webManagerReplyError(QNetworkReply::NetworkError code);

        /** Do actions when async Web Manager requests are finished */
        Q_INVOKABLE void webManagerReplyFinished();

        Q_INVOKABLE TelescopeLite *getTelescope();

        QString connectedHost()
        {
            return m_connectedHost;
        }
        void setConnectedHost(const QString &connectedHost);
        void setConnected(bool connected);

        /**
         * Set the INDI Control Page
         *
         * @param page Reference to the QML page
         */
        void setIndiControlPage(QObject &page)
        {
            indiControlPage = &page;
        };

        /**
         * @brief syncLED
         * @param device device name
         * @param property property name
         * @param name of Light which LED needs to be synced
         * @return color of state
         */
        Q_INVOKABLE QString syncLED(const QString &device, const QString &property, const QString &name = "");

        void buildTextGUI(INDI::Property property);
        void buildNumberGUI(INDI::Property property);
        void buildMenuGUI(INDI::Property property);
        void buildSwitchGUI(INDI::Property property, PGui guiType);
        void buildSwitch(bool buttonGroup, ISwitch *sw, INDI::Property property, bool exclusive = false,
                         PGui guiType = PG_BUTTONS);
        void buildLightGUI(INDI::Property property);
        //void buildBLOBGUI(INDI::Property property);

        Q_INVOKABLE void sendNewINDISwitch(const QString &deviceName, const QString &propName, const QString &name);
        Q_INVOKABLE void sendNewINDISwitch(const QString &deviceName, const QString &propName, int index);

        Q_INVOKABLE void sendNewINDINumber(const QString &deviceName, const QString &propName, const QString &numberName,
                                           double value);
        Q_INVOKABLE void sendNewINDIText(const QString &deviceName, const QString &propName, const QString &fieldName,
                                         const QString &text);

        bool isConnected()
        {
            return m_connected;
        }
        Q_INVOKABLE bool isDeviceConnected(const QString &deviceName);

        QList<DeviceInfoLite *> getDevices()
        {
            return m_devices;
        }

        Q_INVOKABLE QString getLastUsedServer();
        Q_INVOKABLE void setLastUsedServer(const QString &server);

        Q_INVOKABLE int getLastUsedPort();
        Q_INVOKABLE void setLastUsedPort(int port);

        Q_INVOKABLE int getLastUsedWebManagerPort();
        Q_INVOKABLE void setLastUsedWebManagerPort(int port);

        /**
         * @brief saveDisplayImage
         * @return true if image was saved false otherwise
         */
        Q_INVOKABLE bool saveDisplayImage();

        void clearDevices();

    private slots:

        void connectNewDevice(const QString &device_name);

    protected:
        virtual void newDevice(INDI::BaseDevice *dp) override;
        virtual void removeDevice(INDI::BaseDevice *dp) override;
        virtual void newProperty(INDI::Property property) override;
        virtual void removeProperty(INDI::Property property) override;
        virtual void newBLOB(IBLOB *bp) override;
        virtual void newSwitch(ISwitchVectorProperty *svp) override;
        virtual void newNumber(INumberVectorProperty *nvp) override;
        virtual void newMessage(INDI::BaseDevice *dp, int messageID) override;
        virtual void newText(ITextVectorProperty *tvp) override;
        virtual void newLight(ILightVectorProperty *lvp) override;
        virtual void serverConnected() override {}
        virtual void serverDisconnected(int exit_code) override;
    signals:
        //Device
        void newINDIDevice(QString deviceName);
        void removeINDIDevice(QString deviceName);
        void deviceConnected(QString deviceName, bool isConnected);

        void newINDIProperty(QString deviceName, QString propName, QString groupName, QString type, QString label);

        void createINDIText(QString deviceName, QString propName, QString propLabel, QString fieldName, QString propText,
                            bool read, bool write);

        void createINDINumber(QString deviceName, QString propName, QString propLabel, QString numberName, QString propText,
                              bool read, bool write, bool scale);

        void createINDIButton(QString deviceName, QString propName, QString propText, QString switchName, bool read,
                              bool write, bool exclusive, bool checked, bool checkable);
        void createINDIRadio(QString deviceName, QString propName, QString propText, QString switchName, bool read,
                             bool write, bool exclusive, bool checked, bool enabled);

        void createINDIMenu(QString deviceName, QString propName, QString switchLabel, QString switchName, bool isSelected);

        void createINDILight(QString deviceName, QString propName, QString label, QString lightName);

        void removeINDIProperty(QString deviceName, QString groupName, QString propName);

        //Update signals
        void newINDISwitch(QString deviceName, QString propName, QString switchName, bool isOn);
        void newINDINumber(QString deviceName, QString propName, QString numberName, QString value);
        void newINDIText(QString deviceName, QString propName, QString fieldName, QString text);
        void newINDIMessage(QString message);
        void newINDILight(QString deviceName, QString propName);
        void newINDIBLOBImage(QString deviceName, bool isLoaded);
        void newLEDState(QString deviceName, QString propName); // to sync LED for properties

        void connectedHostChanged(QString);
        void connectedChanged(bool);
        void telescopeAdded(TelescopeLite *newTelescope);
        void telescopeRemoved(TelescopeLite *delTelescope);
        void telescopeConnected(TelescopeLite *telescope);
        void telescopeDisconnected();

        void lastUsedServerChanged();
        void lastUsedPortChanged();
        void lastUsedWebManagerPortChanged();

    private:
        bool processBLOBasCCD(IBLOB *bp);

        /// Qml context
        QQmlContext &context;
        QList<DeviceInfoLite *> m_devices;
        QString m_connectedHost;
        bool m_connected { false };
        char BLOBFilename[MAXINDIFILENAME];
        QImage displayImage;
        /// INDI Control Page
        QObject* indiControlPage;
        /// Manager for the JSON requests to the Web Manager
        QNetworkAccessManager manager;
        /// Network reply for querying profiles from the Web Manager
        std::unique_ptr<QNetworkReply> webMProfilesReply;
        /// Network reply for Web Manager status
        std::unique_ptr<QNetworkReply> webMStatusReply;
        /// Network reply to stop the active profile in the Web Manager
        std::unique_ptr<QNetworkReply> webMStopProfileReply;
        /// Network reply to start a profile in the Web Manager
        std::unique_ptr<QNetworkReply> webMStartProfileReply;
        /// Web Manager profiles
        QStringList webMProfiles;
        TelescopeLite *m_telescope { nullptr };
#ifdef ANDROID
        QString defaultImageType;
        QString defaultImagesLocation;
#endif
};
