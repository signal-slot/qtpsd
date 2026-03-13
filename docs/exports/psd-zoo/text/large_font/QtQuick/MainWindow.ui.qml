import QtQuick

Item {
    height: 200
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 67
        source: "images/big.png"
        width: 92
        x: 56
        y: 98
    }
}
