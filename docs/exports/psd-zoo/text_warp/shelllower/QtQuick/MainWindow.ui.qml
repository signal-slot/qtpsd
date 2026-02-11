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
    Text {
        color: "#000000"
        height: 47
        text: ""
        width: 260
        x: 35
        y: 93
    }
}
