import QtQuick

Item {
    height: 300
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 261
        horizontalAlignment: Text.AlignLeft
        text: "First paragraph.\n\nSecond paragraph.\n\nThird paragraph."
        width: 360
        wrapMode: Text.Wrap
        x: 20
        y: 19
    }
}
