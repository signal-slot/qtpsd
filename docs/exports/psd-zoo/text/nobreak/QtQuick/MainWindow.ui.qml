import QtQuick

Item {
    height: 200
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 180
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 180
        wrapMode: Text.Wrap
        x: 10
        y: 10
    }
}
