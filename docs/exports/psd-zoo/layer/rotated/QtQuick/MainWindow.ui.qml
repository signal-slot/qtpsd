import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 213
        source: "images/rotated.png"
        width: 212
        x: 44
        y: 44
    }
}
