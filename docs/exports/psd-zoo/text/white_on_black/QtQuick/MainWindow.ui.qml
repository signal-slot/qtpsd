import QtQuick

Item {
    height: 200
    width: 300
    Rectangle {
        color: "#ff000000"
        height: 200
        width: 300
        x: 0
        y: 0
    }
    Text {
        clip: true
        color: "#ffffffff"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 48
        horizontalAlignment: Text.AlignLeft
        text: "White Text"
        verticalAlignment: Text.AlignBottom
        width: 168
        x: 50
        y: 82
    }
}
