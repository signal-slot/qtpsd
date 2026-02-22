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
        color: "#ff000000"
        font.family: "Roboto"
        font.italic: true
        font.pixelSize: 36
        height: 48
        horizontalAlignment: Text.AlignLeft
        text: "Italic"
        verticalAlignment: Text.AlignBottom
        width: 79
        x: 50
        y: 62
    }
}
