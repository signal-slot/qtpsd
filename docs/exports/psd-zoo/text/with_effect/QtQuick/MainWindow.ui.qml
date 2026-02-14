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
        color: "#c83232"
        font.family: "Roboto"
        font.pixelSize: 48
        height: 54
        horizontalAlignment: Text.AlignLeft
        text: "STYLED"
        width: 173
        x: 20
        y: 90
    }
}
