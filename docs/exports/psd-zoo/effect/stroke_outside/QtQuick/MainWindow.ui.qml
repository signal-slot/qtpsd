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
    Rectangle {
        border.color: "#ff000000"
        border.width: 4
        color: "transparent"
        height: 128
        width: 128
        x: 36
        y: 36
        Image {
            fillMode: Image.PreserveAspectFit
            height: 120
            source: "images/stroke_outside.png"
            width: 120
            x: 4
            y: 4
        }
    }
}
