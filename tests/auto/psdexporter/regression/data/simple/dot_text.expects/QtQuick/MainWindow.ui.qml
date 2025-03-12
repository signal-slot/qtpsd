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
            font.family: "SourceHanSans-Medium"
            font.pointSize: 20
            height: 58
            horizontalAlignment: Text.AlignHCenter
            text: ".\n "
            verticalAlignment: Text.AlignVCenter
            width: 9
            x: 125
            y: 80
        }
    }
}
