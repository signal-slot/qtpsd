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
            color: "#ff1b868a"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Text {
            anchors.fill: parent
            clip: true
            color: "#ff000000"
            font.family: "Source Han Sans"
            font.pixelSize: 50
            horizontalAlignment: Text.AlignHCenter
            text: ""
            verticalAlignment: Text.AlignVCenter
        }
        Item {
            height: 100
            width: 242
            x: 29
            y: 19
            Rectangle {
                color: "#ffa8f6ed"
                height: 97
                width: 240
                x: 1
                y: 0.999995
            }
        }
        Item {
            height: 52
            width: 242
            x: 29
            y: 19
            Rectangle {
                color: "#ff7f64f6"
                height: 50
                width: 240
                x: 1
                y: 0.999995
            }
        }
        Text {
            clip: true
            color: "#ff000000"
            font.family: "Source Han Sans"
            font.pixelSize: 50
            height: 12
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 0.24
            text: "shooting"
            verticalAlignment: Text.AlignVCenter
            width: 239
            x: 30
            y: 6
        }
    }
}
