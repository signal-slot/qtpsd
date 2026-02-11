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
        height: 152
        width: 152
        x: 24
        y: 24
        Rectangle {
            color: "#000000"
            height: 150
            width: 150
            x: 1
            y: 1
        }
    }
}
