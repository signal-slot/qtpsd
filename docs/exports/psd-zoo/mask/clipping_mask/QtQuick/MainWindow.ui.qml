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
        height: 100
        source: "images/base_layer.png"
        width: 100
        x: 50
        y: 50
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/clipped_layer.png"
        width: 200
        x: 0
        y: 0
    }
}
