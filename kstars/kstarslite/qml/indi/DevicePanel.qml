import QtQuick 2.4
import QtQuick.Window 2.2
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import org.kde.kirigami 1.0 as Kirigami

KSPage {
    id: devicesPage
    contentItem: deviceTabView
    TabView {
        id: deviceTabView

        property var groups: []
        property var properties: []
        property var tabs: []

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
                if(devicesPage.title === deviceName) {
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
                                property.name = propName
                                property.label = label
                                property.device = deviceName
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
                            if(columnItem.children[c].name === propName) {
                                columnItem.children[c].destroy()
                            }
                        }
                        if(columnItem.children.length == 0) {
                            var groups = deviceTabView.groups
                            groups.splice(groups.indexOf(groupName), 1)
                            tab.destroy()
                        }
                    }
                }
            }
        }
    }
}
