import QtQuick

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#1b868a"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Text {
            anchors.fill: parent
            color: "#000000"
            font.family: "源ノ角ゴシック JP"
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
                color: "#a8f6ed"
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
                color: "#7f64f6"
                height: 50
                width: 240
                x: 1
                y: 0.999995
            }
        }
        Text {
            color: "#000000"
            font.family: "源ノ角ゴシック JP"
            font.pixelSize: 50
            height: 96
            horizontalAlignment: Text.AlignHCenter
            text: "shooting"
            verticalAlignment: Text.AlignVCenter
            width: 239
            x: 30
            y: 20
        }
    }
}
