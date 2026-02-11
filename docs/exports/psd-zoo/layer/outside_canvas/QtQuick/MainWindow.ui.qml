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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/outside_canvas.png"
        width: 200
        x: -100
        y: -50
    }
}
