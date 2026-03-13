import QtQuick

Item {
    height: 1920
    width: 1080
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1920
        source: "images/background.png"
        width: 1080
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 36
        source: "images/portrait.png"
        width: 154
        x: 404
        y: 925
    }
}
