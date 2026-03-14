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
        border.color: "#ffc89632"
        border.width: 8
        color: "transparent"
        height: 216
        width: 216
        x: -8
        y: -8
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/stroke_emboss.png"
            width: 200
            x: 8
            y: 8
        }
    }
}
