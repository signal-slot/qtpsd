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
        height: 162
        width: 162
        x: 19
        y: 19
        Rectangle {
            color: "#000000"
            height: 160
            radius: 80
            width: 160
            x: 1
            y: 1
        }
    }
}
