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
    Rectangle {
        border.color: "#008000"
        border.width: 5
        color: "transparent"
        height: 200
        width: 200
        x: 0
        y: 0
        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/stroke_center.png"
            width: 200
        }
    }
}
