import QtQuick

Item {
    height: 512
    width: 512
    Image {
        fillMode: Image.PreserveAspectFit
        height: 512
        source: "images/background.png"
        width: 512
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 512
        source: "images/content.png"
        width: 512
        x: 0
        y: 0
    }
}
