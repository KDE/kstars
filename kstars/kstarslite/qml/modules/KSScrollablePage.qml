import QtQuick 2.4
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami

Kirigami.ScrollablePage {
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    anchors.fill: parent
    visible: false

    property Item contentItem: null
    property Item prevPage: null

    function showPage() {
        prevPage = mainWindow.currentPage
        prevPage.visible = false
        visible = true
        mainWindow.currentPage = this
    }

    function goBack() {
        prevPage.visible = true
        visible = false
        mainWindow.currentPage = prevPage
        prevPage = null
    }

    Rectangle {
        id: headerRect
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Screen.height * 0.05
        color: "#2d5b9a"

        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10

            Text {
                id: backButton
                visible: prevPage !== null
                text: "Back"
                color: "white"


                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        goBack()
                    }
                }
            }

            Text {
                visible: prevPage !== null
                text: "Back"
                color: "white"
                anchors.left: backButton.right
                anchors.leftMargin: 20
            }
        }
    }

    Component.onCompleted: {
        if(contentItem !== null) {
            contentItem.anchors.top = headerRect.bottom
            contentItem.anchors.left = contentItem.parent.left
            contentItem.anchors.right = contentItem.parent.right
            contentItem.anchors.leftMargin = 25
            contentItem.anchors.rightMargin = 25
        }
    }
}
