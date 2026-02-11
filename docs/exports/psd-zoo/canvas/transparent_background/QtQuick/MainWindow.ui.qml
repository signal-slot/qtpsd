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
        source: "images/floating_square.png"
        width: 100
        x: 50
        y: 50
    }
}
