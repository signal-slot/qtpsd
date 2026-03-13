import QtQuick

Item {
    height: 240
    width: 640
    Item {
        id: 1
        height: 240
        width: 640
        x: 0
        y: 0
        Rectangle {
            color: "#ff012401"
            height: 240
            width: 640
            x: 0
            y: 0
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 48
            source: "images/12pt.png"
            width: 115
            x: 34
            y: 37
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 143
            source: "images/150px.png"
            width: 464
            x: 162
            y: 82
        }
    }
}
