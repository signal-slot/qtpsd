import QtQuick

Item {
    height: 200
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 27
        horizontalAlignment: Text.AlignLeft
        text: "Normal Bold Italic"
        width: 190
        x: 20
        y: 75
    }
}
