import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 22
        source: "images/top.png"
        width: 40
        x: 100
        y: 33
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 19
        source: "images/middle.png"
        width: 70
        x: 102
        y: 132
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 18
        source: "images/bottom.png"
        width: 76
        x: 102
        y: 233
    }
}
