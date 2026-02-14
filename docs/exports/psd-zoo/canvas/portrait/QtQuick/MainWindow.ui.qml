import QtQuick

Item {
    height: 1920
    width: 1080
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1920
        source: "images/background.png"
        width: 1080
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 48
        height: 54
        horizontalAlignment: Text.AlignLeft
        text: "Portrait"
        width: 159
        x: 400
        y: 910
    }
}
