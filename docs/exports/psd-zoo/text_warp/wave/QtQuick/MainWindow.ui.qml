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
        font.pixelSize: 36
        height: 41
        horizontalAlignment: Text.AlignLeft
        text: "Wave Text"
        width: 164
        x: 80
        y: 112
    }
}
