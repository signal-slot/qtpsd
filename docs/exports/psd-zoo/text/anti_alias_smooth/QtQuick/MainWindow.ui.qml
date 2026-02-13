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
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 28
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 123
        x: 51
        y: 73
    }
}
