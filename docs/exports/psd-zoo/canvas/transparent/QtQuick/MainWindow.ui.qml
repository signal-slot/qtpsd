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
        height: 200
        source: "images/red_square.png"
        width: 200
        x: 0
        y: 0
    }
}
