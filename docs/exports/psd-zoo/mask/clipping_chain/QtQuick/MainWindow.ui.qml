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
        height: 120
        source: "images/clip_base.png"
        width: 120
        x: 40
        y: 40
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/clipped_1.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 100
        source: "images/clipped_2.png"
        width: 200
        x: 0
        y: 0
    }
}
