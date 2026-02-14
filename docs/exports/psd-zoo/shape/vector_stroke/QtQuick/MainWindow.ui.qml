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
        height: 146
        width: 146
        x: 27
        y: 27
        Rectangle {
            border.color: "#ff0000"
            border.width: 3
            color: "#6496c8"
            height: 143
            width: 143
            x: 1.5
            y: 1.5
        }
    }
}
