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
        border.color: "#ff008000"
        border.width: 5
        color: "transparent"
        height: 205
        width: 205
        x: -3
        y: -3
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/stroke_center.png"
            width: 200
            x: 2.5
            y: 2.5
        }
    }
}
