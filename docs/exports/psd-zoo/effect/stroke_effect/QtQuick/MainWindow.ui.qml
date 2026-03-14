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
    Rectangle {
        border.color: "#ffff0000"
        border.width: 3
        color: "transparent"
        height: 106
        width: 206
        x: 47
        y: 47
        Image {
            fillMode: Image.PreserveAspectFit
            height: 100
            source: "images/stroke_layer.png"
            width: 200
            x: 3
            y: 3
        }
    }
}
