import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Line A\nLine B\nLine C"
        verticalAlignment: Text.AlignBottom
        width: 66
        x: 30
        y: 35
    }
}
