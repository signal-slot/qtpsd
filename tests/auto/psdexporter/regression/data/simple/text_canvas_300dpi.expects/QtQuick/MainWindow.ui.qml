import QtQuick

Item {
    height: 240
    width: 640
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#012401"
            height: 240
            width: 640
            x: 0
            y: 0
        }
        Text {
            color: "#ffffff"
            font.family: "SourceHanSans-Medium"
            font.pixelSize: 50
            height: 54
            horizontalAlignment: Text.AlignHCenter
            text: "12pt"
            width: 120
            x: 30
            y: 30
        }
        Text {
            color: "#ffffff"
            font.family: "SourceHanSans-Medium"
            font.pixelSize: 150
            height: 163
            horizontalAlignment: Text.AlignHCenter
            text: "150px"
            width: 478
            x: 150
            y: 60
        }
    }
}
