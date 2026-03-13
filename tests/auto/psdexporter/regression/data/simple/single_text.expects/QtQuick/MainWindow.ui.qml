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
            height: 32
            source: "images/example1.png"
            width: 155
            x: 53
            y: 81
        }
    }
}
