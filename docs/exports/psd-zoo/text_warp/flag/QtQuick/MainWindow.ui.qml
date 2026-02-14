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
        font.family: "Roboto"
        font.pixelSize: 48
        height: 54
        horizontalAlignment: Text.AlignLeft
        text: "Flag"
        width: 91
        x: 100
        y: 100
    }
}
