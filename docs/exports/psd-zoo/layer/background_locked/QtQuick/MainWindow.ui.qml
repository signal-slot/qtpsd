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
        height: 140
        opacity: 0.701961
        source: "images/overlay.png"
        width: 140
        x: 30
        y: 30
    }
}
