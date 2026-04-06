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
        clip: true
        color: "#ff000000"
        font.bold: true
        font.family: "Roboto"
        font.pixelSize: 28
        height: 37
        horizontalAlignment: Text.AlignLeft
        text: "DISTORT"
        verticalAlignment: Text.AlignBottom
        width: 114
        x: 50
        y: 91
    }
}
