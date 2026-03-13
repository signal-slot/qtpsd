import QtQuick

Item {
    height: 500
    width: 500
    Image {
        fillMode: Image.PreserveAspectFit
        height: 500
        source: "images/background.png"
        width: 500
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 20
        source: "images/offset.png"
        width: 65
        x: 301
        y: 381
    }
}
