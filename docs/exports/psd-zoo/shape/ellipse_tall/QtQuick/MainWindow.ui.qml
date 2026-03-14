import QtQuick

Item {
    height: 300
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 262
        source: "images/tall_ellipse.png"
        width: 102
        x: 49
        y: 19
    }
}
