import QtQuick 2.6
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import "../constants" 1.0

ListView {
    id: listView
    clip: true
    property bool checkCurrent: false

    onCountChanged: {
        var root = listView.visibleChildren[0]
        if(root) {
            var listViewHeight = 0
            var listViewWidth = 0

            // iterate over each delegate item to get their sizes
            // FIX IT: We don't add the height of last child because list becomes longer than we need
            for (var i = 0; i < root.visibleChildren.length - 1; i++) {
                var childWidth = root.visibleChildren[i].textWidth
                if(childWidth > listViewWidth) listViewWidth = childWidth
                listViewHeight += root.visibleChildren[i].height
            }
            listView.implicitWidth = listViewWidth
            listView.implicitHeight = listViewHeight
        }
    }

    ScrollIndicator.vertical: ScrollIndicator { }
    property string textRole: ""
    signal clicked(var index)
    property bool modelIsArray: false

    onModelChanged: {
        modelIsArray = !model ? model.constructor === Array : false
    }

    delegate: Rectangle {
        id: delegateRect
        width: parent.width
        height: objName.height + 30
        property int textWidth: objRow.width + objRow.anchors.leftMargin*2

        border {
            color: "#becad5"
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
                    color: "#d0e8fa"
                }
                PropertyChanges {
                    target: objName
                    color: "#31363b"
                }
            },
            State {
                name: "selected"
                PropertyChanges {
                    target: delegateRect
                    color: "#2196F3"
                }
                PropertyChanges {
                    target: objName
                    color: "#eff0fa"
                }
            },
            State {
                name: "default"
                PropertyChanges {
                    target: delegateRect
                    color: "white"
                }
                PropertyChanges {
                    target: objName
                    color: "black"
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
                listView.currentIndex = model.index
                listView.clicked(model.index)
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
                visible: checkCurrent && listView.currentIndex == model.index
                color: "#2173f3"
                width: height
                height: objName.height/2
                radius: width * 0.5
            }

            Text {
                id: objName

                text: textRole == "" ? modelData : listView.modelIsArray ? modelData[textRole] : model[textRole]

                Behavior on color {
                    ColorAnimation  { duration: 200 }
                }
            }

        }
    }
}
