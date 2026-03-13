import QtQuick

Item {
    height: 300
    width: 300
    Rectangle {
        color: "#ffc8c8c8"
        height: 300
        width: 300
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 23
        source: "images/foreground_text.png"
        width: 171
        x: 52
        y: 132
    }
}
