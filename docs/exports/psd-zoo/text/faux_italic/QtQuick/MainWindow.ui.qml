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
        font.pixelSize: 30
        height: 39
        horizontalAlignment: Text.AlignLeft
        text: "Faux Italic"
        verticalAlignment: Text.AlignBottom
        width: 137
        x: 20
        y: 69
    }
}
