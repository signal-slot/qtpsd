import QtQuick

Item {
    height: 250
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 250
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 81
        horizontalAlignment: Text.AlignLeft
        text: "Tall"
        width: 57
        x: 100
        y: 82
    }
}
