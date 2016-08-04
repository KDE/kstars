import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Dialogs 1.2
import "../constants" 1.0
import "../modules"
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami

/*AbstractDialog {
    id: findDialog
    default property alias data: defaultContentItem.data
    onVisibilityChanged: if (visible && contentItem) contentItem.forceActiveFocus()

    Rectangle {
        id: content
        property real minimumHeight: 320 * num.dp
        property real minimumWidth: 360 * num.dp

        color: sysPalette.window

        ColumnLayout {
            id: defaultContentItem
            Layout.fillWidth: true
            Layout.fillHeight: true
        }*/
KSPage {
    contentItem: findColumn
    title: "Find Object"
    onVisibleChanged: {
        if(visible) {
            typeChoose.currentIndex = 0
            searchQuery.text = ""
        }
    }

    ColumnLayout {
        id: findColumn
        spacing: 5 * num.dp
        anchors{
            bottom: parent.bottom
            bottomMargin: 15 * num.dp
        }

        RowLayout {
            anchors {
                left: parent.left
                right: parent.right
            }
            Controls.Label {
                text: xi18n("Filter by name: ")
                color: num.sysPalette.text
            }
            Controls.TextField {
                id: searchQuery
                Layout.fillWidth: true
                onTextChanged: {
                    FindDialogLite.filterList(text)
                }
            }
        }

        RowLayout {
            anchors {
                left: parent.left
                right: parent.right
            }
            Controls.Label {
                text: "Filter by type: "
            }

            Item {
                //Spacer
                Layout.minimumWidth: 30 * num.dp
                Layout.fillWidth: true
            }

            Controls.ComboBox {
                id: typeChoose
                model: FindDialogLite.filterModel
                Layout.fillWidth: true

                //Init list with objects when they are loaded
                Connections {
                    target: mainWindow
                    onLoadedChanged: {
                        if(loaded) {
                            FindDialogLite.filterByType(typeChoose.currentIndex)
                        }
                    }
                }

                onCurrentIndexChanged: {
                    if(loaded) FindDialogLite.filterByType(currentIndex)
                }
            }
        }

        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list
                clip: true

                model: SortModel

                delegate: Rectangle {
                    id: delRect
                    Layout.fillWidth: true
                    width: parent.width
                    height: objName.height + 30 * num.dp

                    border {
                        color: "#becad5"
                        width: 1 * num.dp
                    }

                    Behavior on color {
                        ColorAnimation  { duration: 200 }
                    }

                    states: [
                        State {
                            name: "hovered"
                            PropertyChanges {
                                target: delRect
                                color: "#d0e8fa"
                            }
                            PropertyChanges {
                                target: objName
                                color: Kirigami.Theme.viewTextColor
                            }
                        },
                        State {
                            name: "selected"
                            PropertyChanges {
                                target: delRect
                                color: Kirigami.Theme.highlightColor
                            }
                            PropertyChanges {
                                target: objName
                                color: Kirigami.Theme.highlightedTextColor
                            }
                        },
                        State {
                            name: "default"
                            PropertyChanges {
                                target: delRect
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
                                delRect.state = "selected"
                            } else {
                                if(containsMouse) {
                                    delRect.state = "hovered"
                                } else {
                                    delRect.state = "default"
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
                            initPage.showPage()
                            FindDialogLite.selectObject(model.index)
                        }
                    }

                    Text {
                        id: objName
                        text: model.name
                        anchors {
                            verticalCenter: parent.verticalCenter
                            left: parent.left
                            leftMargin: 20 * num.dp
                        }
                        Behavior on color {
                            ColorAnimation  { duration: 200 }
                        }
                    }
                }
            }
        }

        Controls.Button {
            text: "Cancel"
            onClicked: {
                goBack()
            }

            anchors {
                bottom: parent.bottom
                right: parent.right
            }
        }
    }
}

