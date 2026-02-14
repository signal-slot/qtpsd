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
        height: 142
        width: 142
        x: 29
        y: 29
        Rectangle {
            color: "#ff0080"
            height: 140
            width: 140
            x: 0.999995
            y: 0.999995
        }
    }
}
