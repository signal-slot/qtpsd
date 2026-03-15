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
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 48
        height: 63
        horizontalAlignment: Text.AlignLeft
        text: "Portrait"
        verticalAlignment: Text.AlignBottom
        width: 159
        x: 400
        y: 910
    }
}
