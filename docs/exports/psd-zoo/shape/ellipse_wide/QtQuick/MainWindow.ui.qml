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
        height: 102
        width: 262
        x: 19
        y: 49
        Rectangle {
            color: "#ff8000"
            height: 100
            radius: 130
            width: 260
            x: 0.999999
            y: 1
        }
    }
}
