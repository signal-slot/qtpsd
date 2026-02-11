import QtQuick

Item {
    height: 300
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        height: 33
        text: ""
        width: 162
        x: 81
        y: 122
    }
}
