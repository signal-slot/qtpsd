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
        height: 182
        width: 182
        x: 9
        y: 9
        Rectangle {
            color: "#8000ff"
            height: 180
            radius: 90
            width: 180
            x: 1
            y: 1
        }
    }
}
