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
        font.underline: true
        height: 48
        horizontalAlignment: Text.AlignLeft
        text: "Underline"
        verticalAlignment: Text.AlignBottom
        width: 151
        x: 30
        y: 62
    }
}
