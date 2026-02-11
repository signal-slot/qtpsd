import QtQuick

Item {
    height: 1024
    width: 1024
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1024
        source: "images/background.png"
        width: 1024
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1024
        source: "images/content.png"
        width: 1024
        x: 0
        y: 0
    }
}
