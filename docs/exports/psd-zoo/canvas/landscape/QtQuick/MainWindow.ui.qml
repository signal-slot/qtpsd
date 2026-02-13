import QtQuick

Item {
    height: 1080
    width: 1920
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1080
        source: "images/background.png"
        width: 1920
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 46
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 229
        x: 804
        y: 504
    }
}
