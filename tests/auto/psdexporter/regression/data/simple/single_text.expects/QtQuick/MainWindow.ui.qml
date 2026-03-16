import QtQuick

Item {
    height: 240
    width: 320
    Item {
        id: 1
        height: 240
        width: 320
        x: 0
        y: 0
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
            font.weight: 500
            height: 44
            horizontalAlignment: Text.AlignHCenter
            text: "Example1"
            verticalAlignment: Text.AlignVCenter
            width: 160
            x: 50
            y: 71
        }
    }
}
