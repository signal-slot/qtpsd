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
    Text {
        color: "#000000"
        height: 8
        text: ""
        width: 53
        x: 10
        y: 44
    }
}
