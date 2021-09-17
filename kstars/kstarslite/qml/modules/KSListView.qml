// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import "../constants/" 1.0

ListView {
    id: listView
    clip: true
    property bool checkCurrent: false
    property bool checkable: false
    property bool onClickCheck: true //true if the item should be be mapped with a blue rectangle when clicked

    property int maxWidth: 0
    property bool modelIsChanged: false
    implicitWidth: maxWidth
    //To skip the case when contentItem.height equals to 99000+
    implicitHeight: contentItem.height >= window.height ? window.height : contentItem.height

    //For some reason we can't use num constant inside states so we wrap sysPalette as property
    property SystemPalette sysPalette: Num.sysPalette

    onCountChanged: {
        for(var child in listView.contentItem.children) {
            var childWidth = listView.contentItem.children[child].textWidth
            if(childWidth == undefined) childWidth = 0
            maxWidth = maxWidth > childWidth ? maxWidth : childWidth
        }
    }

    ScrollIndicator.vertical: ScrollIndicator { }
    property string textRole: ""

    signal clicked(var text, var index, var checked)
    property bool modelIsArray: false

    onModelChanged: {
        modelIsArray = !model ? model.constructor === Array : false
    }

    delegate: Rectangle {
        id: delegateRect
        width: parent.width
        height: objName.contentHeight + 30
        property int textWidth: objRow.width + objRow.anchors.leftMargin*2
        property bool checked: false
        property string visibleText: objName.text
        color: sysPalette.base

        border {
            color: Num.sysPalette.light//"#becad5"
            width: 1
        }

        Behavior on color {
            ColorAnimation  { duration: 200 }
        }

        states: [
            State {
                name: "hovered"
                PropertyChanges {
                    target: delegateRect
                    color: sysPalette.highlight //"#d0e8fa"
                }
                PropertyChanges {
                    target: objName
                    color: sysPalette.highlightedText //"#31363b"
                }
            },
            State {
                name: "selected"
                PropertyChanges {
                    target: delegateRect
                    color: sysPalette.button//"#2196F3"
                }
                PropertyChanges {
                    target: objName
                    color: sysPalette.buttonText//"#eff0fa"
                }
            },
            State {
                name: "default"
                PropertyChanges {
                    target: delegateRect
                    color: sysPalette.base//"white"
                }
                PropertyChanges {
                    target: objName
                    color: sysPalette.text//"black"
                }
            }
        ]

        MouseArea {
            id: delMouseArea
            anchors.fill: parent
            hoverEnabled: true

            function hoveredColor() {
                if(pressed) {
                    delegateRect.state = "selected"
                } else {
                    if(containsMouse) {
                        delegateRect.state = "hovered"
                    } else {
                        delegateRect.state = "default"
                    }
                }
            }

            onContainsMouseChanged: {
                hoveredColor()
            }

            onPressedChanged: {
                hoveredColor()
            }

            onClicked: {
                if(onClickCheck) listView.currentIndex = model.index
                if(checkable) delegateRect.checked = !delegateRect.checked
                listView.clicked(objName.text, model.index, delegateRect.checked)
            }
        }

        RowLayout {
            id: objRow

            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 20
            }

            Rectangle {
                visible: (checkCurrent && listView.currentIndex == model.index) || (checkable && delegateRect.checked)
                color: objName.color //"#2173f3"
                width: height
                height: objName.font.pixelSize/2
                radius: width * 0.5
            }

            KSText {
                id: objName

                text: textRole == "" ? modelData : listView.modelIsArray ? modelData[textRole] : model[textRole]
                wrapMode: Text.WrapAnywhere

                Behavior on color {
                    ColorAnimation  { duration: 200 }
                }
            }
        }
    }
}
