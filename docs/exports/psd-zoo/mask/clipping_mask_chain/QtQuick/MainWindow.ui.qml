import QtQuick

Item {
    height: 200
    width: 200
    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: "images/"
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 140
        source: "images/base.png"
        width: 140
        x: 30
        y: 30
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/clip_1.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 100
        opacity: 0.501961
        source: "images/clip_2.png"
        width: 200
        x: 0
        y: 0
    }
}
