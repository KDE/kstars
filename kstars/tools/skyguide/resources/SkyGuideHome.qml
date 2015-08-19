/***************************************************************************
                          SkyGuideHome.qml  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/07/01
    copyright            : (C) 2015 by Marcos Cardinot
    email                : mcardinot@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

Rectangle {
    color: "#00020e"

    Image {
        width: parent.width
        height: parent.height
        fillMode: Image.PreserveAspectCrop
        source: "images/background.jpeg"
    }

    ColumnLayout {
        id: home
        spacing: 10
        anchors.fill: parent

        Text {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "#ffffff"
            font.pixelSize: 30
            style: Text.Outline
            text: "Sky Guide"
            font.bold: true
        }

        ListView {
            id: guidesList
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.9
            spacing: 7
            focus: true
            clip: true
            model: guidesModel
            delegate: guidesDelegate
            Keys.onReturnPressed: goToPage(getPageObj('INFO', currentItem.modelData, -1))
        }

        Component {
            id: guidesDelegate
            Rectangle {
                property var modelData: model.modelData
                width: guidesList.width
                height: 25

                color: ListView.isCurrentItem ? "#157efb" : "#39475C"
                radius: 5

                Text {
                    anchors.fill: parent
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    color: "#ffffff"
                    text: title
                    style: Text.Sunken
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onDoubleClicked: goToPage(getPageObj('INFO', model.modelData, -1))
                    onClicked: {
                        guidesList.currentIndex = index;
                        if (mouse.button == Qt.RightButton) {
                            ctxMenu.popup();
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: ctxMenu
        MenuItem {
            text: "Uninstall"
            onTriggered: uninstall(guidesList.currentIndex)
        }
    }
}
