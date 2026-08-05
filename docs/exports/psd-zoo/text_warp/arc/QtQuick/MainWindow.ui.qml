import QtQuick

Item {
    height: 300
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 62
        source: "images/warp_arc.png"
        width: 226
        x: 56
        y: 87
    }
}
