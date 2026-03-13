import QtQuick

Item {
    height: 4000
    width: 4000
    Image {
        fillMode: Image.PreserveAspectFit
        height: 4000
        source: "images/background.png"
        width: 4000
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 67
        source: "images/large_canvas.png"
        width: 425
        x: 1806
        y: 1948
    }
}
