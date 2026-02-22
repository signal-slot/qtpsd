import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 21
        height: 28
        horizontalAlignment: Text.AlignLeft
        text: "Rotated"
        verticalAlignment: Text.AlignBottom
        width: 99
        x: 71
        y: 88
    }
}
