import QtQuick

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#fff7e790"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Text {
            clip: true
            color: "#ff000000"
            font.family: "Source Han Sans"
            font.pixelSize: 30
            height: 44
            horizontalAlignment: Text.AlignHCenter
            text: "."
            verticalAlignment: Text.AlignVCenter
            width: 9
            x: 125
            y: 71
        }
    }
}
