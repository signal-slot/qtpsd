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
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 50
        height: 66
        horizontalAlignment: Text.AlignLeft
        text: "150 DPI"
        verticalAlignment: Text.AlignBottom
        width: 175
        x: 30
        y: 48
    }
}
