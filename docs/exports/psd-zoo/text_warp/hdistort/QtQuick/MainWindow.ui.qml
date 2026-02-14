import QtQuick

Item {
    height: 200
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 28
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "DISTORT"
        width: 114
        x: 50
        y: 91
    }
}
