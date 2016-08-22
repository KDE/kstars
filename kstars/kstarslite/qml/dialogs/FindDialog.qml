import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0
import "../constants" 1.0
import "../modules"

KSPage {
    contentItem: findColumn
    title: xi18n("Find an Object")
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
            Label {
                text: xi18n("Filter by name: ")
            }
            TextField {
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
            Label {
                text: "Filter by type: "
            }

            Item {
                //Spacer
                Layout.minimumWidth: 30 * num.dp
                Layout.fillWidth: true
            }

            ComboBox {
                id: typeChoose
                model: FindDialogLite.filterModel
                Layout.fillWidth: true

                //Init list with objects when everything is loaded
                Connections {
                    target: window
                    onLoaded: {
                        if(isLoaded) FindDialogLite.filterByType(typeChoose.currentIndex)
                    }
                }

                onCurrentIndexChanged: {
                    if(isLoaded) FindDialogLite.filterByType(currentIndex)
                }
            }
        }

        KSListView {
            model: SortModel
            textRole: "name"

            Layout.fillWidth: true
            Layout.fillHeight: true

            onClicked: {
                stackView.replace(null, initPage)
                FindDialogLite.selectObject(index)
            }
        }

        Button {
            text: "Cancel"
            onClicked: {
                stackView.pop()
            }

            anchors {
                bottom: parent.bottom
                right: parent.right
            }
        }
    }
}

