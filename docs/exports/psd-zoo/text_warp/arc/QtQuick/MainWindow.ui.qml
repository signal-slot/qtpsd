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
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 48
        horizontalAlignment: Text.AlignLeft
        text: "Warped Arc"
        verticalAlignment: Text.AlignBottom
        width: 186
        x: 80
        y: 112
    }
}
