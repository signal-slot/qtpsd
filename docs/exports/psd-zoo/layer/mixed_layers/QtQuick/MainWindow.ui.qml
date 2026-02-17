import QtQuick

Item {
    height: 300
    width: 300
    Rectangle {
        color: "#c8c8c8"
        height: 300
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000080"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Foreground Text"
        verticalAlignment: Text.AlignBottom
        width: 174
        x: 50
        y: 125
    }
}
