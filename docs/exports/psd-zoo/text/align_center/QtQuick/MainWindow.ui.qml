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
        clip: true
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignHCenter
        text: "Center Align"
        verticalAlignment: Text.AlignBottom
        width: 130
        x: 85
        y: 75
    }
}
