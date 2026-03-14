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
        border.width: 5
        color: "transparent"
        height: 210
        width: 210
        x: -5
        y: -5
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/grad_stroke.png"
            width: 200
            x: 5
            y: 5
        }
    }
}
