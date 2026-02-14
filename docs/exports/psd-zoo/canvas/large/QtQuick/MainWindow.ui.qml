import QtQuick

Item {
    height: 4000
    width: 4000
    Image {
        fillMode: Image.PreserveAspectFit
        height: 4000
        source: "images/background.png"
        width: 4000
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 72
        height: 81
        horizontalAlignment: Text.AlignLeft
        text: "Large Canvas"
        width: 435
        x: 1800
        y: 1925
    }
}
