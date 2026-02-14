import QtQuick

Item {
    height: 500
    width: 500
    Image {
        fillMode: Image.PreserveAspectFit
        height: 500
        source: "images/background.png"
        width: 500
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 27
        horizontalAlignment: Text.AlignLeft
        text: "Offset"
        width: 66
        x: 300
        y: 375
    }
}
