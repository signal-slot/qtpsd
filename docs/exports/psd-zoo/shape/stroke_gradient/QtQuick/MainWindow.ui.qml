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
        height: 148
        width: 148
        x: 26
        y: 26
        Rectangle {
            height: 140
            width: 140
            x: 4
            y: 4
        }
    }
}
