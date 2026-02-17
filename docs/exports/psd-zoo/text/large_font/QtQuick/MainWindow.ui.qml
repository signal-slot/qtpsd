import QtQuick

Item {
    height: 200
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 72
        height: 95
        horizontalAlignment: Text.AlignLeft
        text: "Big"
        verticalAlignment: Text.AlignBottom
        width: 103
        x: 50
        y: 75
    }
}
