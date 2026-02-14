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
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 41
        horizontalAlignment: Text.AlignLeft
        text: "TWIST"
        width: 108
        x: 50
        y: 82
    }
}
