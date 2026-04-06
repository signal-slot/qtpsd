import QtQuick

Item {
    height: 200
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Regular"
        verticalAlignment: Text.AlignBottom
        width: 87
        x: 20
        y: 75
    }
    Text {
        clip: true
        color: "#ff000000"
        font.bold: true
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Bold"
        verticalAlignment: Text.AlignBottom
        width: 55
        x: 145
        y: 75
    }
}
