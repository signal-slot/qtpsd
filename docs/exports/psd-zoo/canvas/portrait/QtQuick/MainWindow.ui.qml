import QtQuick

Item {
    height: 1920
    width: 1080
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1920
        source: "images/background.png"
        width: 1080
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 36
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 154
        x: 404
        y: 925
    }
}
