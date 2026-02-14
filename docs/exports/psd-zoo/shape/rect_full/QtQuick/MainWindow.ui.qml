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
        height: 202
        width: 202
        x: -1
        y: -1
        Rectangle {
            color: "#0080ff"
            height: 200
            width: 200
            x: 1
            y: 1
        }
    }
}
