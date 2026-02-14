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
        border.color: "#ff0000"
        border.width: 5
        color: "transparent"
        height: 140
        width: 140
        x: 30
        y: 30
        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            height: 140
            source: "images/stroke_inside.png"
            width: 140
        }
    }
}
