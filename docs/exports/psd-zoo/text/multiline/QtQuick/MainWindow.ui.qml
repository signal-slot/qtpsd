import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        height: 103
        text: ""
        width: 109
        x: 52
        y: 62
    }
}
