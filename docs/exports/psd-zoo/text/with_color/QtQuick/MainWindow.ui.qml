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
        color: "#ffff0000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Red Text"
        verticalAlignment: Text.AlignBottom
        width: 92
        x: 50
        y: 75
    }
}
