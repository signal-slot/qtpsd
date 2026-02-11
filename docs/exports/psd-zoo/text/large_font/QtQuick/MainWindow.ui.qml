import QtQuick

Item {
    height: 200
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        height: 67
        text: ""
        width: 92
        x: 56
        y: 98
    }
}
