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
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 48
        horizontalAlignment: Text.AlignLeft
        text: "Small Caps"
        verticalAlignment: Text.AlignBottom
        width: 193
        x: 20
        y: 62
    }
}
