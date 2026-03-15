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
        font.pixelSize: 200
        height: 264
        horizontalAlignment: Text.AlignLeft
        text: "600 DPI"
        verticalAlignment: Text.AlignBottom
        width: 699
        x: 30
        y: -110
    }
}
