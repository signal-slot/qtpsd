import QtQuick

Item {
    height: 250
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 250
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 55
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 53
        x: 101
        y: 66
    }
}
