import QtQuick

Item {
    height: 200
    width: 2000
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 2000
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/content.png"
        width: 2000
        x: 0
        y: 0
    }
}
