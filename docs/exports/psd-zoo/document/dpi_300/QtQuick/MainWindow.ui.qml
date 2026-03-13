import QtQuick

Item {
    height: 600
    width: 600
    Image {
        fillMode: Image.PreserveAspectFit
        height: 600
        source: "images/background.png"
        width: 600
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 37
        source: "images/300_dpi.png"
        width: 169
        x: 52
        y: 64
    }
}
