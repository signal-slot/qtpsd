import QtQuick

Item {
    height: 240
    width: 320
    Item {
        id: 1
        height: 240
        width: 320
        x: 0
        y: 0
        Rectangle {
            color: "#fff7e790"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 5
            source: "images/.png"
            width: 5
            x: 127
            y: 101
        }
    }
}
