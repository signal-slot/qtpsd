import QtQuick

Item {
    height: 2000
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 2000
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 2000
        source: "images/content.png"
        width: 200
        x: 0
        y: 0
    }
}
