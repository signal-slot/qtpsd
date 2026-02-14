import QtQuick

Item {
    height: 400
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 400
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 81
        horizontalAlignment: Text.AlignLeft
        text: "Vertical"
        width: 24
        x: 88
        y: 25
    }
}
