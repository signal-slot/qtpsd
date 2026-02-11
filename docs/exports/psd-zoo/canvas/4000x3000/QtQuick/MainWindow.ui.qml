import QtQuick

Item {
    height: 3000
    width: 4000
    Image {
        fillMode: Image.PreserveAspectFit
        height: 3000
        source: "images/background.png"
        width: 4000
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 3000
        source: "images/content.png"
        width: 4000
        x: 0
        y: 0
    }
}
