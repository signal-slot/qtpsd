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
        font.pixelSize: 16
        height: 252
        horizontalAlignment: Text.AlignLeft
        text: "Paragraph text in a bounding box. This text wraps automatically when it reaches the edge of the box."
        width: 354
        wrapMode: Text.Wrap
        x: 25
        y: 23
    }
}
