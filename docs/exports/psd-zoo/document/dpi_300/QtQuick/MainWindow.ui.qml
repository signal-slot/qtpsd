import QtQuick

Item {
    height: 600
    width: 600
    Image {
        fillMode: Image.PreserveAspectFit
        height: 600
        source: "images/background.png"
        width: 600
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 37
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 169
        x: 52
        y: 64
    }
}
