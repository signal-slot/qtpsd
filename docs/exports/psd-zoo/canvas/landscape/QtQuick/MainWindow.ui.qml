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
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 48
        height: 63
        horizontalAlignment: Text.AlignLeft
        text: "Landscape"
        verticalAlignment: Text.AlignBottom
        width: 234
        x: 800
        y: 490
    }
}
