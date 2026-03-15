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
    Item {
        height: 162
        width: 262
        x: 19
        y: 19
        Rectangle {
            color: "#ff0064c8"
            height: 160
            width: 260
            x: 0.999999
            y: 1
        }
    }
    Text {
        clip: true
        color: "#ffffffff"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignHCenter
        text: "CLICK ME"
        verticalAlignment: Text.AlignBottom
        width: 107
        x: 21
        y: 90
    }
}
