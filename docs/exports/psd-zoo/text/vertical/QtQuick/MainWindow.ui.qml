import QtQuick

Item {
    height: 400
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 400
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 80
        source: "images/vertical.png"
        width: 19
        x: 91
        y: 50
    }
}
