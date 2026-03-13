import QtQuick

Item {
    height: 1080
    width: 1920
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1080
        source: "images/background.png"
        width: 1920
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 46
        source: "images/landscape.png"
        width: 229
        x: 804
        y: 504
    }
}
