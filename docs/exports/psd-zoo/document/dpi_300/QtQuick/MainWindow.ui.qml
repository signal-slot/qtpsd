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
        font.family: "Roboto"
        font.pixelSize: 50
        height: 56
        horizontalAlignment: Text.AlignLeft
        text: "300 DPI"
        width: 175
        x: 50
        y: 48
    }
}
