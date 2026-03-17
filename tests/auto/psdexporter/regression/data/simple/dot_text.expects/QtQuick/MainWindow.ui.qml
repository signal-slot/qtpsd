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
            height: 49
            horizontalAlignment: Text.AlignHCenter
            text: "."
            verticalAlignment: Text.AlignVCenter
            width: 9
            x: 125
            y: 71
        }
    }
}
