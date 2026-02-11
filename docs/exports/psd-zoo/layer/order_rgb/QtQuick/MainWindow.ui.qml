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
        height: 100
        source: "images/red_bottom.png"
        width: 100
        x: 20
        y: 20
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 100
        source: "images/green_middle.png"
        width: 100
        x: 50
        y: 50
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 100
        source: "images/blue_top.png"
        width: 100
        x: 80
        y: 80
    }
}
