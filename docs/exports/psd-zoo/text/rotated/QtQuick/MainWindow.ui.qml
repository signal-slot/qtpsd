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
        height: 88
        text: ""
        width: 89
        x: 90
        y: 96
    }
}
