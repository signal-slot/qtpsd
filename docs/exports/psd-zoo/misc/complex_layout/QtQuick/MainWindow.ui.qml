import QtQuick

Item {
    height: 300
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 300
            source: "images/bg_color.png"
            width: 400
            x: 0
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Item {
            height: 242
            width: 302
            x: 49
            y: 29
            Rectangle {
                color: "#000000"
                height: 240
                width: 300
                x: 1
                y: 1.00001
            }
        }
        Text {
            color: "#000000"
            font.family: "Sans Serif"
            font.pixelSize: 9
            height: 16
            horizontalAlignment: Text.AlignLeft
            text: ""
            width: 87
            x: 80
            y: 65
        }
        Text {
            color: "#000000"
            font.family: "Sans Serif"
            font.pixelSize: 9
            height: 33
            horizontalAlignment: Text.AlignLeft
            text: ""
            width: 242
            x: 80
            y: 110
        }
    }
}
