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
            height: 140
            width: 140
            x: 3
            y: 3
        }
    }
}
