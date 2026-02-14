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
    Item {
        height: 148
        width: 148
        x: 26
        y: 26
        Rectangle {
            border.color: "#000000"
            border.width: 5
            color: "#e6e6e6"
            height: 145
            width: 145
            x: 1.5
            y: 1.5
        }
    }
}
