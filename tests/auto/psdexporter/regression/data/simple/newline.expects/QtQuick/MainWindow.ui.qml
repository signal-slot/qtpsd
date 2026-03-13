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
            color: "#ff012401"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Text {
            anchors.fill: parent
            clip: true
            color: "#ffeac3c3"
            font.family: "Source Han Sans"
            font.pixelSize: 50
            horizontalAlignment: Text.AlignHCenter
            text: ""
            verticalAlignment: Text.AlignVCenter
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 82
            source: "images/fcb0f0a0956c3f6d.png"
            width: 157
            x: 2
            y: 10
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 82
            source: "images/4beb81a176ce2747.png"
            width: 141
            x: 170
            y: 10
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 80
            source: "images/f46b2f6f02ec82d9.png"
            width: 84
            x: 26
            y: 112
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 107
            source: "images/cea43522b47e00f0.png"
            width: 146
            x: 167
            y: 110
        }
    }
}
