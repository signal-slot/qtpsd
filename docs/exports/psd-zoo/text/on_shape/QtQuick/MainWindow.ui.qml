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
            color: "#000000"
            height: 160
            width: 260
            x: 0.999999
            y: 1
        }
    }
    Text {
        color: "#000000"
        height: 19
        text: ""
        width: 106
        x: 22
        y: 97
    }
}
