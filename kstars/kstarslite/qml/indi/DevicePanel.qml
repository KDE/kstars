import QtQuick 2.4
import QtQuick.Window 2.2
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import org.kde.kirigami 1.0 as Kirigami

Item {
    id: devicesPanel
    anchors.fill: parent
    function showPage(backtoInit) {
        devicesPage.showPage(backtoInit)
    }

    property string deviceName
    property ImagePreview imagePreview: null

    KSPage {
        id: devicesPage
        contentItem: deviceTabView
        title: devicesPanel.deviceName + " - " + deviceTabView.currentTabTitle

        TabView {
            id: deviceTabView
            currentIndex: 0

            property var groups: []
            property var properties: []
            property var tabs: []
            property string currentTabTitle: ""

            onCurrentIndexChanged: {
                currentTabTitle = getTab(currentIndex).title
            }

            Component.onCompleted: {
                if(Qt.platform.os != "android") {
                    anchors.topMargin = 10 * num.dp
                    anchors.leftMargin = 5 * num.dp
                    anchors.rightMargin = 5 * num.dp
                }
                anchors.bottomMargin = 5 * num.dp
                anchors.bottom = parent.bottom
            }

            Connections {
                target: ClientManagerLite
                onNewINDIProperty: {
                    if(deviceTabView.currentTabTitle == "") deviceTabView.currentTabTitle = groupName
                    if(devicesPanel.deviceName === deviceName) {
                        if(deviceTabView.groups.indexOf(groupName) == -1) {
                            deviceTabView.groups.push(groupName)
                            var newTabComp = Qt.createComponent("modules/KSTab.qml");
                            var newTab = newTabComp.createObject(deviceTabView)
                            newTab.title = groupName

                            deviceTabView.tabs.push(newTab)
                            if(groupName == "Motion Control") {
                                var component = Qt.createComponent("modules/MotionControl.qml");
                                var motionControl = component.createObject(newTab)
                                motionControl.deviceName = deviceName
                            }
                        }
                        if(groupName != "Motion Control") {
                            for(var i = 0; i < deviceTabView.count; ++i) {
                                var tab = deviceTabView.getTab(i)
                                if(tab.title === groupName) {
                                    var propComp = Qt.createComponent("modules/Property.qml");
                                    var property = propComp.createObject(tab.columnItem)
                                    property.propName = propName
                                    property.label = label
                                    property.deviceName = deviceName
                                    property.parentTab = tab
                                    if(propName == "CCD_EXPOSURE" && devicesPanel.imagePreview == null) {
                                        var imgPreviewComp = Qt.createComponent("ImagePreview.qml");
                                        devicesPanel.imagePreview = imgPreviewComp.createObject(devicesPanel)
                                        devicesPanel.imagePreview.deviceName = devicesPanel.deviceName
                                    }
                                }
                            }
                        }
                    }
                }
                onRemoveINDIProperty: {
                    for(var i = 0; i < deviceTabView.tabs.length; ++i) {
                        var tab = deviceTabView.tabs[i]
                        if(tab.title === groupName && groupName != "Motion Control") {
                            var columnItem = deviceTabView.tabs[i].columnItem
                            for(var c = 0; c < columnItem.children.length; ++c) {
                                if(columnItem.children[c].propName === propName) {
                                    columnItem.children[c].destroy()
                                }
                            }
                            if(columnItem.children.length == 0) {
                                var groups = deviceTabView.groups
                                groups.splice(groups.indexOf(groupName), 1)
                                tab.destroy()
                            }
                            /*if(propName == "CCD_EXPOSURE" && devicesPanel.imagePreview != null) {
                                imgPreview.destroy()
                            }*/
                        }
                    }
                }
            }
        }
    }
}
