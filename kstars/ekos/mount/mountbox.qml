import QtQuick 2.5
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.1

Item {
    width: 320
    height: 480

    ColumnLayout
    {
        id: columnLayout        

        GridLayout {
            id: gridLayout
            anchors.fill: parent
            rows: 3
            columns: 3

            Button {
                id: neButton
                Image
                {
                    source: "qrc:/icons/go-nw.svg"
                    fillMode: Image.PreserveAspectFit
                    anchors.centerIn: parent
                }
            }

            Button {
                id: button1
            }

            Button {
                id: button2
            }

            Button {
                id: button3
            }

            Button {
                id: button4
            }

            Button {
                id: button5
            }

            Button {
                id: button6
            }

            Button {
                id: button7
            }

            Button {
                id: button8
            }
        }
    }

}
