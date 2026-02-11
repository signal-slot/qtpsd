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
        height: 42
        width: 42
        x: 79
        y: 79
        Rectangle {
            color: "#000000"
            height: 40
            width: 40
            x: 0.999995
            y: 0.999995
        }
    }
}
