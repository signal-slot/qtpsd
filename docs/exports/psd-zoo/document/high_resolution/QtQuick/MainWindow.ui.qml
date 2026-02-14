import QtQuick

Item {
    height: 200
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 100
        height: 113
        horizontalAlignment: Text.AlignLeft
        text: "300 DPI"
        width: 349
        x: 30
        y: -5
    }
}
