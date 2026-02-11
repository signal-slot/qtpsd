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
        height: 150
        source: "images/partial.png"
        width: 150
        x: 25
        y: 25
    }
}
