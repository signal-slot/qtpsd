import QtQuick

Item {
    height: 400
    width: 600
    Image {
        fillMode: Image.PreserveAspectFit
        height: 400
        source: "images/background.png"
        width: 600
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 180
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 553
        x: 19
        y: 20
    }
}
