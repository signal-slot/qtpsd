import QtQuick

Item {
    height: 300
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Item {
        height: 262
        width: 102
        x: 49
        y: 19
        Rectangle {
            color: "#000000"
            height: 260
            radius: 50
            width: 100
            x: 1
            y: 0.999999
        }
    }
}
