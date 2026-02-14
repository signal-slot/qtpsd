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
        font.family: "Roboto"
        font.pixelSize: 48
        height: 54
        horizontalAlignment: Text.AlignLeft
        text: "Landscape"
        width: 234
        x: 800
        y: 490
    }
}
