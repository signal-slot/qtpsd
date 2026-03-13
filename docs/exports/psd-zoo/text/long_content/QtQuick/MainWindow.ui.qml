import QtQuick

Item {
    height: 400
    width: 600
    Image {
        fillMode: Image.PreserveAspectFit
        height: 400
        source: "images/background.png"
        width: 600
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 180
        source: "images/The quick brown fox jumps over the lazy dog. The quick brown fo"
        width: 553
        x: 19
        y: 20
    }
}
