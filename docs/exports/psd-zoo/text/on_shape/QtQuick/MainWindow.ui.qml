import QtQuick

Item {
    height: 200
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Item {
        height: 162
        width: 262
        x: 19
        y: 19
        Rectangle {
            color: "#ff0064c8"
            height: 160
            width: 260
            x: 0.999999
            y: 1
        }
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 19
        source: "images/button_label.png"
        width: 106
        x: 22
        y: 97
    }
}
