import QtQuick

Item {
    height: 200
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/bottom_red.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/middle_green.png"
        width: 200
        x: 20
        y: 20
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/top_blue.png"
        width: 200
        x: 40
        y: 40
    }
}
