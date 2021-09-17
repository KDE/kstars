// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.8
import QtQuick.Window 2.2
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0

KSPage {
    id: devicesPage
    title: devicesPage.deviceName + " - " + tabBar.currentItem.text

    property string deviceName
    property ImagePreview imagePreview: null

    ColumnLayout {
        anchors.fill: parent

        Item {
            anchors {
                left: parent.left
                right: parent.right
            }
            height: tabBar.height

            KSTabBarArrow {
                imgSource: "../images/left-arrow.png"
                tabBar: tabBar
                state: {
                    if(!tabBar.contentItem.atXBeginning) {
                        return "Visible"
                    } else {
                        return "Invisible"
                    }
                }
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
            }

            KSTabBarArrow {
                imgSource: "../images/right-arrow.png"
                tabBar: tabBar
                state: {
                    if(!tabBar.contentItem.atXEnd) {
                        return "Visible"
                    } else {
                        return "Invisible"
                    }
                }
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                flickSpeed: -1000
            }

            TabBar {
                id: tabBar
                Layout.fillHeight: true
                anchors {
                    left: parent.left
                    right: parent.right
                }
                clip: true
                spacing: 20
                Component.onCompleted: {
                    contentItem.flickDeceleration = 1000
                }

                background: Rectangle {
                    color: Num.sysPalette.base
                }
            }
        }

        SwipeView {
            id: deviceSwipeView
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: tabBar.currentIndex
            clip: true

            property var groups: []
            property var properties: []
            property var tabs: []

            onCurrentIndexChanged: {
                tabBar.currentIndex = currentIndex
            }

            Connections {
                target: ClientManagerLite
                onNewINDIProperty: {
                    if(devicesPage.deviceName === deviceName) {
                        if(deviceSwipeView.groups.indexOf(groupName) == -1) {
                            deviceSwipeView.groups.push(groupName)
                            var newTabComp = Qt.createComponent("../modules/KSTab.qml");
                            var newTab = newTabComp.createObject(deviceSwipeView)
                            newTab.title = groupName

                            var columnForTab = Qt.createQmlObject('import QtQuick 2.7
                                                                    import QtQuick.Layouts 1.3
                                                                    Column {
                                                                        spacing: 5
                                                                }', newTab.contentItem)

                            newTab.rootItem = columnForTab
                            var tabButton = Qt.createQmlObject('import QtQuick 2.7;
                                                                import QtQuick.Controls 2.0
                                                                import "../modules"
                                                                KSTabButton {}',
                                                               tabBar);
                            tabButton.text = groupName
                            if(tabBar.count == 1) {
                                //Without notifying about adding first item to tabBar title of devicesPage won't be updated
                                tabBar.currentItemChanged()
                            }

                            deviceSwipeView.tabs.push(newTab)
                            if(groupName == "Motion Control") {
                                var component = Qt.createComponent("modules/MotionControl.qml");
                                var motionControl = component.createObject(newTab)
                                motionControl.deviceName = deviceName
                            }
                        }

                        if(groupName != "Motion Control") {
                            for(var i = 0; i < deviceSwipeView.tabs.length; ++i) {
                                var tab = deviceSwipeView.tabs[i]
                                if(tab.title === groupName) {
                                    var propComp = Qt.createComponent("modules/Property.qml");
                                    var property = propComp.createObject(tab.rootItem)
                                    property.propName = propName
                                    property.label = label
                                    property.deviceName = deviceName
                                    property.parentTab = tab
                                    if(propName == "CCD_EXPOSURE" && devicesPage.imagePreview == null) {
                                        var imgPreviewComp = Qt.createComponent("ImagePreview.qml");
                                        devicesPage.imagePreview = imgPreviewComp.createObject(window)
                                        devicesPage.imagePreview.deviceName = devicesPage.deviceName
                                    }
                                }
                            }
                        }
                    }
                }

                onRemoveINDIProperty: {
                    for(var i = 0; i < deviceSwipeView.tabs.length; ++i) {
                        var tab = deviceSwipeView.tabs[i]
                        if(tab.title === groupName && groupName != "Motion Control") {
                            var contentItem = deviceSwipeView.tabs[i].rootItem
                            for(var c = 0; c < contentItem.children.length; ++c) {
                                if(contentItem.children[c].propName === propName) {
                                    contentItem.children[c].destroy()
                                }
                            }
                            if(contentItem.children.length == 0) {
                                var groups = deviceSwipeView.groups
                                groups.splice(groups.indexOf(groupName), 1)
                                tab.destroy()
                            }
                            /*if(propName == "CCD_EXPOSURE" && devicesPage.imagePreview != null) {
                                    imgPreview.destroy()
                                }*/
                        }
                    }
                }
            }
        }
    }
}
