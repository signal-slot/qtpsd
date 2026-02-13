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
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 62
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 226
        x: 56
        y: 87
    }
}
