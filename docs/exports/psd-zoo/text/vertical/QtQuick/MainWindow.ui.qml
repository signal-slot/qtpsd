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
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 80
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 19
        x: 91
        y: 50
    }
}
