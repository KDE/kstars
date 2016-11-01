// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.2
import "../modules"
import "../constants" 1.0

KSPage {
    id: indiPage
    objectName: "indiControlPanel"
    title: "INDI Control Panel"

    property bool connected: ClientManagerLite.connected

//    Component.onCompleted: {
//        if(Qt.platform.os != "android") {
//            ClientManagerLite.setHost("localhost", parseInt(7624))
//        }
//    }

    onConnectedChanged: {
        if(!indiPage.connected) {
            for(var i = 0; i < devicesModel.count; ++i) {
                devicesModel.get(i).panel.destroy()
                stackView.pop(indiPage)
            }
            devicesModel.clear()
            notification.showNotification("Disconnected from the server")
        }
    }
    contentItem: cPanelColumn

    ColumnLayout {
        id: cPanelColumn
        spacing: 5 * num.dp

        ColumnLayout {
            visible: !indiPage.connected
            anchors {
                left: parent.left
                right: parent.right
            }

            KSLabel {
                text: xi18n("INDI Host")
            }

            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                KSTextField {
                    id:ipHost
                    placeholderText: xi18n("IP")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width*0.8
                    Layout.fillWidth: true
                    text: ClientManagerLite.lastUsedServer
                }

                KSTextField {
                    id:portHost
                    placeholderText: xi18n("Port")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width*0.2
                    Layout.fillWidth: true
                    text: ClientManagerLite.lastUsedPort
                }
            }
        }

        KSLabel {
            id: connectedTo
            visible: indiPage.connected
            text: xi18n("Connected to ") + ClientManagerLite.connectedHost
        }

        Button {
            text: indiPage.connected ? xi18n("Disconnect") : xi18n("Connect ")

            onClicked: {
                if(!indiPage.connected) {
                    if(ClientManagerLite.setHost(ipHost.text, parseInt(portHost.text))) {
                        notification.showNotification(xi18n("Successfully connected to the server"))
                    } else {
                        notification.showNotification(xi18n("Couldn't connect to the server"))
                    }
                } else {

                    ClientManagerLite.disconnectHost()
                }
                Qt.inputMethod.hide()
            }
        }

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            visible : indiPage.connected

            Rectangle {
                Layout.fillWidth: true
                height: 1 * num.dp
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
                    for(i = 0; i < devicesModel.count; ++i) {
                        if(devicesModel.get(i).name == deviceName) {
                            devicesModel.panel.destroy()
                            devicesModel.remove(i)
                        }
                    }
                }
                onNewINDIMessage: {
                    notification.showNotification(message)
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
    }
}
