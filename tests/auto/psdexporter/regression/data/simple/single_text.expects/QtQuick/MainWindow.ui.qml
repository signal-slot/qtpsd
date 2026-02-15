import QtQuick

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#f7e790"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Text {
            color: "#000000"
            font.family: "Source Han Sans"
            font.pixelSize: 30
            height: 33
            horizontalAlignment: Text.AlignHCenter
            text: "Example1"
            verticalAlignment: Text.AlignVCenter
            width: 160
            x: 50
            y: 79
        }
    }
}
