// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.2
import Qt.labs.settings 1.0
import "../modules"
import "../constants" 1.0

KSPage {
    id: indiPage
    objectName: "indiControlPanel"
    title: xi18n("INDI Control Panel")

    property bool connected: ClientManagerLite.connected
    property alias webMStatusText: webMStatusLabel.text
    property alias webMStatusTextVisible: webMStatusLabel.visible
    property alias webMActiveProfileText: webMActiveProfileLabel.text
    property alias webMActiveProfileLayoutVisible: webMActiveProfileLayout.visible
    property alias webMBrowserButtonVisible: webMBrowserButton.visible
    property alias webMProfileListVisible: webMProfileList.visible

    function connectIndiServer() {
        indiServerConnectButton.clicked()
    }

    Component.onCompleted: {
        // Debug purposes
        ClientManagerLite.setHost("localhost", 7624)
    }

    onConnectedChanged: {
        if (!indiPage.connected) {
            for (var i = 0; i < devicesModel.count; ++i) {
                devicesModel.get(i).panel.destroy()
                stackView.pop(indiPage)
            }
            devicesModel.clear()
            skyMapLite.notification.showNotification("Disconnected from the server")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        id: cPanelColumn
        spacing: 5 * Num.dp

        ColumnLayout {
            visible: !indiPage.connected
            anchors {
                left: parent.left
                right: parent.right
            }

            KSLabel {
                text: xi18n("IP Address or Hostname")
            }

            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                KSTextField {
                    id: ipHost
                    placeholderText: xi18n("xxx.xxx.xxx.xxx")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width*0.8
                    Layout.fillWidth: true
                    font.capitalization: Font.AllLowercase
                    //text: ClientManagerLite.lastUsedServer
                    text: "localhost"

                    Settings
                    {
                        property alias ipHostText : ipHost.text
                    }
                }
            }

            KSLabel {
                text: xi18n("Web Manager Port")
            }

            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                KSTextField {
                    id: portWebManager
                    placeholderText: xi18n("xxxx")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width*0.2
                    Layout.fillWidth: true
                    //text: ClientManagerLite.lastUsedWebManagerPort
                    text: "8624"

                    Settings
                    {
                        property alias portWebManagerText : portWebManager.text
                    }
                }

                Button {
                    id: webMConnectButton
                    text: xi18n("Get Status")

                    onClicked: {
                        ClientManagerLite.getWebManagerProfiles(ipHost.text, parseInt(portWebManager.text));
                        Qt.inputMethod.hide()
                    }
                }
            }

            KSLabel {
                id: webMStatusLabel
                text: xi18n("Web Manager Status:")
                visible: false
            }

            RowLayout {
                id: webMActiveProfileLayout
                visible: false

                KSLabel {
                    id: webMActiveProfileLabel
                    text: xi18n("Active Profile:")
                }

                Button {
                    id: webMStopButton
                    text: xi18n("Stop")

                    onClicked: {
                        ClientManagerLite.stopWebManagerProfile();
                    }
                }
            }

            ListView {
                id: webMProfileList
                model: webMProfileModel
                highlightFollowsCurrentItem: false
                width: parent.width
                height: childrenRect.height
                visible: false

                delegate: RowLayout {
                    height: webMConnectButton.height

                    Rectangle {
                        width: webMStatusLabel.width
                        height: webMConnectButton.height
                        KSLabel {
                            text: xi18n("Profile: %1", modelData)
                        }
                    }

                    Button {
                        height: webMConnectButton.height
                        text: xi18n("Start")

                        onClicked: {
                            ClientManagerLite.startWebManagerProfile(modelData);
                        }
                    }
                }
            } // ListView

            Button {
                id: webMBrowserButton
                text: xi18n("Manage Profiles")
                visible: false

                onClicked: {
                    Qt.openUrlExternally("http://"+ipHost.text+":"+portWebManager.text)
                }
            }

            KSLabel {
                text: xi18n("Server Port")
            }

            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                KSTextField {
                    id: portHost
                    placeholderText: xi18n("INDI Server Port")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width*0.2
                    Layout.fillWidth: true
                    //text: ClientManagerLite.lastUsedPort
                    text: "7624"

                    Settings
                    {
                        property alias portHostText : portHost.text
                    }
                }

                Button {
                    id: indiServerConnectButton
                    text: indiPage.connected ? xi18n("Disconnect") : xi18n("Connect")

                    onClicked: {
                        if (!indiPage.connected) {
                            if(ClientManagerLite.setHost(ipHost.text, parseInt(portHost.text))) {
                                skyMapLite.notification.showNotification(xi18n("Successfully connected to the server"))
                            } else {
                                skyMapLite.notification.showNotification(xi18n("Could not connect to the server"))
                            }
                        } else {

                            ClientManagerLite.disconnectHost()
                        }
                        Qt.inputMethod.hide()
                    }
                }

            }
        }

        KSLabel {
            id: connectedTo
            visible: indiPage.connected
            text: xi18n("Connected to %1", ClientManagerLite.connectedHost)
        }


        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            visible: indiPage.connected

            Rectangle {
                Layout.fillWidth: true
                height: 1 * Num.dp
                color: "gray"
            }

            KSLabel {
                id: devicesLabel
                text: xi18n("Available Devices")
            }

            ListModel {
                id: devicesModel
            }

            Connections {
                target: ClientManagerLite
                onNewINDIDevice: {
                    var component = Qt.createComponent(Qt.resolvedUrl("./DevicePanel.qml"));
                    var devicePanel = component.createObject(window);
                    devicePanel.deviceName = deviceName
                    devicesModel.append({ name: deviceName, panel: devicePanel })
                }
                onRemoveINDIDevice: {
                    for (i = 0; i < devicesModel.count; ++i) {
                        if(devicesModel.get(i).name == deviceName) {
                            devicesModel.panel.destroy()
                            devicesModel.remove(i)
                        }
                    }
                }
                onNewINDIMessage: {
                    skyMapLite.notification.showNotification(message)
                }
            }
        }

        KSListView {
            id: devicesPage
            Layout.fillHeight: true
            Layout.fillWidth: true

            model: devicesModel
            textRole: "name"

            onClicked: {
                stackView.push(devicesModel.get(currentIndex).panel)
            }
        }

        KSButton
        {
            id: disconnectINDI
            visible: indiPage.connected
            text: xi18n("Disconnect INDI");

            onClicked:
            {
                ClientManagerLite.disconnectHost();
            }
        }
    }
}
