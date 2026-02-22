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
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 48
        horizontalAlignment: Text.AlignLeft
        text: "Tall"
        verticalAlignment: Text.AlignBottom
        width: 57
        x: 100
        y: 82
    }
}
