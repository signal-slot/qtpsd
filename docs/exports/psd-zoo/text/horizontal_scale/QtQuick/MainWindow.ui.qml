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
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 41
        horizontalAlignment: Text.AlignLeft
        text: "Stretched"
        width: 231
        x: 20
        y: 62
    }
}
