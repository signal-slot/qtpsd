import QtQuick

Item {
    height: 240
    width: 640
    Item {
        id: 1
        height: 240
        width: 640
        x: 0
        y: 0
        Rectangle {
            color: "#ff012401"
            height: 240
            width: 640
            x: 0
            y: 0
        }
        Text {
            clip: true
            color: "#ffffffff"
            font.family: "Source Han Sans"
            font.pixelSize: 50
            font.weight: 500
            height: 72
            horizontalAlignment: Text.AlignHCenter
            text: "12pt"
            verticalAlignment: Text.AlignVCenter
            width: 120
            x: 30
            y: 16
        }
        Text {
            clip: true
            color: "#ffffffff"
            font.family: "Source Han Sans"
            font.pixelSize: 150
            font.weight: 500
            height: 217
            horizontalAlignment: Text.AlignHCenter
            text: "150px"
            verticalAlignment: Text.AlignVCenter
            width: 478
            x: 150
            y: 18
        }
    }
}
