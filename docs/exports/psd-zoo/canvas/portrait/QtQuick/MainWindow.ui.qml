import QtQuick

Item {
    height: 1920
    width: 1080
    Image {
        fillMode: Image.PreserveAspectFit
        height: 1920
        source: "images/background.png"
        width: 1080
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        height: 36
        text: ""
        width: 154
        x: 404
        y: 925
    }
}
