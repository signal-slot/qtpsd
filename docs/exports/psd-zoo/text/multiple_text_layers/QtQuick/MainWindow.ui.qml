import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 22
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 40
        x: 100
        y: 33
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 70
        x: 102
        y: 132
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 18
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 76
        x: 102
        y: 233
    }
}
