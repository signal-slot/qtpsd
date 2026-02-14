import QtQuick

Item {
    height: 200
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Rectangle {
        border.color: "#ffff00"
        border.width: 2
        color: "transparent"
        height: 100
        width: 200
        x: 50
        y: 50
        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            height: 100
            source: "images/multi_effect.png"
            width: 200
        }
    }
}
